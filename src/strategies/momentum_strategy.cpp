#include "strategies/momentum_strategy.h"
#include "managers/position_manager.h"
#include "managers/risk_manager.h"
#include "config/config_manager.h"
#include "utils/logger.h"
#include <cmath>
#include <sstream>
#include <numeric>
#include <algorithm>

MomentumStrategy::MomentumStrategy() 
    : StrategyBase("MomentumStrategy") {
    LOG_INFO("MomentumStrategy initialized - chase momentum mode");
}

void MomentumStrategy::onScanResult(const ScanResult& result) {
    if (!running_) return;
    
    const auto& params = ConfigManager::getInstance().getConfig().strategy.momentum;
    auto& pos_mgr = PositionManager::getInstance();
    
    // 如果已经持仓，跳过
    if (pos_mgr.hasPosition(result.symbol)) {
        return;
    }
    
    // 检查是否已达最大持仓数
    auto positions = pos_mgr.getAllPositions();
    int active_count = 0;
    for (const auto& p : positions) {
        if (p.second.quantity > 0) active_count++;
    }
    if (active_count >= 5) {
        return;  // 最多同时持有5只票
    }
    
    std::stringstream ss;
    ss << "Breakout detected: " << result.symbol 
       << " price=" << result.price 
       << " chg=" << (result.change_ratio * 100) << "%"
       << " volR=" << result.volume_ratio
       << " amp=" << (result.amplitude * 100) << "%"
       << " score=" << result.score;
    LOG_INFO(ss.str());
    
    // 获取5分钟K线做趋势确认
    auto klines = getHistoryKLine(result.symbol, "K_5M", 50);
    
    if (klines.size() < 5) {
        LOG_WARN("Insufficient kline data for " + result.symbol);
        return;
    }
    
    // 判断是否应该追涨入场
    if (shouldEnter(result, klines)) {
        // 订阅实时数据用于追踪止盈止损
        subscribeStock(result.symbol);
        
        // 计算买入数量
        int quantity = calculateQuantity(result.symbol, result.price);
        
        if (quantity > 0) {
            // 市价追入
            if (buy(result.symbol, quantity, 0.0)) {
                std::lock_guard<std::mutex> lock(chase_mutex_);
                auto& entry = chase_entries_[result.symbol];
                entry.entry_price = result.price;
                entry.high_water_mark = result.price;
                entry.entry_volume_ratio = result.volume_ratio;
                entry.entry_score = result.score;
                entry.entry_time_ms = currentTimeMs();
                
                std::stringstream log_ss;
                log_ss << "CHASE ENTER: " << result.symbol 
                       << " qty=" << quantity 
                       << " price=" << result.price
                       << " volRatio=" << result.volume_ratio
                       << " score=" << result.score;
                LOG_INFO(log_ss.str());
            }
        }
    }
}

void MomentumStrategy::onKLine(const std::string& symbol, const KlineData& kline) {
    StrategyBase::onKLine(symbol, kline);

    if (!running_) return;
    
    const auto& params = ConfigManager::getInstance().getConfig().strategy.momentum;
    auto& pos_mgr = PositionManager::getInstance();
    Position* pos = pos_mgr.getPosition(symbol);
    if (!pos || pos->quantity <= 0) return;
    
    std::lock_guard<std::mutex> lock(chase_mutex_);
    auto it = chase_entries_.find(symbol);
    if (it == chase_entries_.end()) return;
    
    auto& entry = it->second;
    double current_price = kline.close_price;
    
    // 更新最高水位
    entry.high_water_mark = std::max(entry.high_water_mark, kline.high_price);
    
    // 计算各种退出指标
    double pnl_ratio = (current_price - entry.entry_price) / entry.entry_price;
    double drawdown_from_high = (entry.high_water_mark - current_price) / entry.high_water_mark;
    double elapsed_min = (currentTimeMs() - entry.entry_time_ms) / 60000.0;
    
    bool should_exit = false;
    std::string exit_reason;
    
    // 1. 硬止损 - 亏损超过阈值直接砍
    if (pnl_ratio <= -params.chase_hard_stop_loss) {
        should_exit = true;
        exit_reason = "HARD_STOP_LOSS (" + std::to_string(pnl_ratio * 100) + "%)";
    }
    
    // 2. 追踪止盈 - 从最高点回撤超过阈值
    if (!should_exit && pnl_ratio > 0 && drawdown_from_high >= params.chase_trailing_stop) {
        should_exit = true;
        exit_reason = "TRAILING_STOP (high=" + std::to_string(entry.high_water_mark) + 
                      " dd=" + std::to_string(drawdown_from_high * 100) + "%)";
    }
    
    // 3. 目标止盈 - 达到目标涨幅
    if (!should_exit && pnl_ratio >= params.chase_take_profit) {
        should_exit = true;
        exit_reason = "TAKE_PROFIT (" + std::to_string(pnl_ratio * 100) + "%)";
    }
    
    // 4. 动量衰竭 - K线连续缩量下跌
    if (!should_exit && kline.close_price < kline.open_price) {
        // 阴线，且距入场已有盈利但动量消退
        double kline_drop = (kline.open_price - kline.close_price) / kline.open_price;
        if (kline_drop > 0.01 && pnl_ratio > 0) {
            should_exit = true;
            exit_reason = "MOMENTUM_FADE (kline_drop=" + std::to_string(kline_drop * 100) + "%)";
        }
    }
    
    // 5. 超时退出 - 超过N分钟没有明显涨幅
    if (!should_exit && elapsed_min >= params.momentum_stale_minutes && pnl_ratio < 0.01) {
        should_exit = true;
        exit_reason = "STALE_MOMENTUM (" + std::to_string((int)elapsed_min) + "min, pnl=" + 
                      std::to_string(pnl_ratio * 100) + "%)";
    }
    
    if (should_exit) {
        sell(symbol, pos->quantity, 0.0);
        unsubscribeStock(symbol);
        
        std::stringstream ss;
        ss << "CHASE EXIT: " << symbol 
           << " reason=" << exit_reason
           << " entry=" << entry.entry_price 
           << " exit=" << current_price
           << " pnl=" << (pnl_ratio * 100) << "%";
        LOG_INFO(ss.str());
        
        chase_entries_.erase(it);
    }
}

void MomentumStrategy::onTick(const std::string& symbol, const TickData& tick) {
    StrategyBase::onTick(symbol, tick);

    if (!running_) return;
    
    // Tick级别更新最高水位（更高频追踪）
    std::lock_guard<std::mutex> lock(chase_mutex_);
    auto it = chase_entries_.find(symbol);
    if (it != chase_entries_.end()) {
        it->second.high_water_mark = std::max(it->second.high_water_mark, tick.last_price);
    }
}

void MomentumStrategy::onSnapshot(const Snapshot& snapshot) {
    StrategyBase::onSnapshot(snapshot);

    if (!running_) return;
    
    auto& pos_mgr = PositionManager::getInstance();
    pos_mgr.updateMarketPrice(snapshot.symbol, snapshot.last_price);
    
    // 对追涨持仓做实时检查
    Position* pos = pos_mgr.getPosition(snapshot.symbol);
    if (!pos || pos->quantity <= 0) return;
    
    const auto& params = ConfigManager::getInstance().getConfig().strategy.momentum;
    
    std::lock_guard<std::mutex> lock(chase_mutex_);
    auto it = chase_entries_.find(snapshot.symbol);
    if (it == chase_entries_.end()) return;
    
    auto& entry = it->second;
    double current_price = snapshot.last_price;
    
    // 更新最高水位
    entry.high_water_mark = std::max(entry.high_water_mark, current_price);
    
    double pnl_ratio = (current_price - entry.entry_price) / entry.entry_price;
    
    // 实时硬止损检查（snapshot比K线更及时）
    if (pnl_ratio <= -params.chase_hard_stop_loss) {
        sell(snapshot.symbol, pos->quantity, 0.0);
        unsubscribeStock(snapshot.symbol);
        
        std::stringstream ss;
        ss << "REALTIME STOP: " << snapshot.symbol 
           << " price=" << current_price
           << " loss=" << (pnl_ratio * 100) << "%";
        LOG_INFO(ss.str());
        
        chase_entries_.erase(it);
    }
}

bool MomentumStrategy::shouldEnter(const ScanResult& result, const std::vector<KlineData>& klines) {
    const auto& params = ConfigManager::getInstance().getConfig().strategy.momentum;
    
    // 1. 量比必须达标（爆发的核心指标）
    if (result.volume_ratio < params.breakout_volume_ratio) {
        return false;
    }
    
    // 2. 涨幅在合理追涨区间
    if (result.change_ratio < params.breakout_change_min || 
        result.change_ratio > params.breakout_change_max) {
        return false;
    }
    
    // 3. RSI不能过高过低
    double rsi = calculateRSI(klines);
    if (rsi > params.chase_rsi_max || rsi < params.chase_rsi_min) {
        return false;
    }
    
    // 4. 距离日内最高价不能太远（避免追在最高点）
    if (result.price_vs_high > params.price_vs_high_max && result.price_vs_high > 0) {
        LOG_INFO(result.symbol + " rejected: too far from high (" + 
                 std::to_string(result.price_vs_high * 100) + "%)");
        return false;
    }
    
    // 5. 买卖盘比例要健康（买盘 > 卖盘说明有资金在托）
    if (result.bid_ask_ratio < 0.8) {
        return false;
    }
    
    // 6. 短期趋势确认 - 最近几根K线要有向上动量
    // 检查最近3根K线是否有升势
    bool recent_up = false;
    if (klines.size() >= 3) {
        size_t n = klines.size();
        recent_up = (klines[n-1].close_price > klines[n-3].close_price);
    }
    if (!recent_up) {
        return false;
    }
    
    std::stringstream ss;
    ss << "Entry confirmed: " << result.symbol 
       << " volR=" << result.volume_ratio 
       << " rsi=" << rsi 
       << " b/a=" << result.bid_ask_ratio
       << " vsHigh=" << (result.price_vs_high * 100) << "%";
    LOG_INFO(ss.str());
    
    return true;
}

bool MomentumStrategy::shouldChaseExit(const std::string& symbol, double current_price, double speed) {
    const auto& params = ConfigManager::getInstance().getConfig().strategy.momentum;
    
    std::lock_guard<std::mutex> lock(chase_mutex_);
    auto it = chase_entries_.find(symbol);
    if (it == chase_entries_.end()) return false;
    
    auto& entry = it->second;
    double pnl_ratio = (current_price - entry.entry_price) / entry.entry_price;
    double drawdown = (entry.high_water_mark - current_price) / entry.high_water_mark;
    
    // 动量反转退出
    if (speed < params.momentum_exit_speed && pnl_ratio > 0) {
        return true;
    }
    
    // 追踪止盈
    if (pnl_ratio > 0.005 && drawdown >= params.chase_trailing_stop) {
        return true;
    }
    
    return false;
}

int MomentumStrategy::calculateQuantity(const std::string& symbol, double price) {
    (void)symbol;
    
    auto& risk_mgr = RiskManager::getInstance();
    const auto& config = ConfigManager::getInstance().getConfig();
    
    // 根据总资金的20%分配单票仓位
    double position_budget = config.trading.max_position_size * 0.2;
    
    int quantity = risk_mgr.calculatePositionSize(price, position_budget);
    
    // 港股最小单位100股
    quantity = (quantity / 100) * 100;
    if (quantity < 100) quantity = 100;
    
    return quantity;
}

int64_t MomentumStrategy::currentTimeMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

double MomentumStrategy::calculateRSI(const std::vector<KlineData>& klines, int period) {
    if (klines.size() < static_cast<size_t>(period + 1)) {
        return 50.0;  // 默认值
    }
    
    double gain_sum = 0.0;
    double loss_sum = 0.0;
    
    // 计算平均涨跌
    for (size_t i = klines.size() - period; i < klines.size(); ++i) {
        double change = klines[i].close_price - klines[i - 1].close_price;
        if (change > 0) {
            gain_sum += change;
        } else {
            loss_sum += std::abs(change);
        }
    }
    
    double avg_gain = gain_sum / period;
    double avg_loss = loss_sum / period;
    
    if (avg_loss == 0) return 100.0;
    
    double rs = avg_gain / avg_loss;
    double rsi = 100.0 - (100.0 / (1.0 + rs));
    
    return rsi;
}

double MomentumStrategy::calculateMACD(const std::vector<KlineData>& klines) {
    // 简化的MACD计算
    if (klines.size() < 26) {
        return 0.0;
    }
    
    // 计算12日和26日EMA
    double ema12 = 0.0;
    double ema26 = 0.0;
    
    for (size_t i = klines.size() - 12; i < klines.size(); ++i) {
        ema12 += klines[i].close_price;
    }
    ema12 /= 12;
    
    for (size_t i = klines.size() - 26; i < klines.size(); ++i) {
        ema26 += klines[i].close_price;
    }
    ema26 /= 26;
    
    return ema12 - ema26;
}

bool MomentumStrategy::isUptrend(const std::vector<KlineData>& klines) {
    if (klines.size() < 20) {
        return false;
    }
    
    // 计算20日移动平均线
    double ma20 = 0.0;
    for (size_t i = klines.size() - 20; i < klines.size(); ++i) {
        ma20 += klines[i].close_price;
    }
    ma20 /= 20;
    
    // 当前价格在均线上方
    double current_price = klines.back().close_price;
    if (current_price < ma20) {
        return false;
    }
    
    // 均线向上
    double ma20_prev = 0.0;
    for (size_t i = klines.size() - 25; i < klines.size() - 5; ++i) {
        ma20_prev += klines[i].close_price;
    }
    ma20_prev /= 20;
    
    return ma20 > ma20_prev;
}

