#include "strategies/momentum_strategy.h"
#include "managers/position_manager.h"
#include "managers/risk_manager.h"
#include "config/config_manager.h"
#include "utils/logger.h"
#include <cmath>
#include <sstream>
#include <numeric>

MomentumStrategy::MomentumStrategy() 
    : StrategyBase("MomentumStrategy") {
}

void MomentumStrategy::onScanResult(const ScanResult& result) {
    if (!running_) return;
    
    std::stringstream ss;
    ss << "Processing scan result: " << result.symbol 
       << " price=" << result.price << " change=" << result.change_ratio;
    LOG_INFO(ss.str());
    
    auto& pos_mgr = PositionManager::getInstance();
    
    // 如果已经持仓，跳过
    if (pos_mgr.hasPosition(result.symbol)) {
        return;
    }
    
    // 获取历史K线数据进行分析
    auto klines = getHistoryKLine(result.symbol, "K_5M", 100);
    
    if (klines.empty()) {
        LOG_WARNING("No historical data for " + result.symbol);
        return;
    }
    
    // 判断是否应该入场
    if (shouldEnter(result, klines)) {
        // 订阅实时数据
        subscribeStock(result.symbol);
        
        // 计算买入数量
        int quantity = calculateQuantity(result.symbol, result.price);
        
        if (quantity > 0) {
            // 市价买入
            if (buy(result.symbol, quantity, 0.0)) {
                entry_prices_[result.symbol] = result.price;
                
                std::stringstream log_ss;
                log_ss << "Entered position: " << result.symbol 
                       << " qty=" << quantity << " price=" << result.price;
                LOG_INFO(log_ss.str());
            }
        }
    }
}

void MomentumStrategy::onKLine(const std::string& symbol, const KlineData& kline) {
    StrategyBase::onKLine(symbol, kline);

    if (!running_) return;
    
    // 检查是否需要止盈或止损
    auto& risk_mgr = RiskManager::getInstance();
    
    if (risk_mgr.shouldStopLoss(symbol, kline.close_price) ||
        risk_mgr.shouldTakeProfit(symbol, kline.close_price)) {
        
        // 平仓
        auto& pos_mgr = PositionManager::getInstance();
        Position* pos = pos_mgr.getPosition(symbol);
        
        if (pos && pos->quantity > 0) {
            sell(symbol, pos->quantity, 0.0);
            
            // 取消订阅
            unsubscribeStock(symbol);
            entry_prices_.erase(symbol);
            
            std::stringstream ss;
            ss << "Exited position: " << symbol 
               << " P/L=" << pos->profit_loss;
            LOG_INFO(ss.str());
        }
    }
}

void MomentumStrategy::onTick(const std::string& symbol, const TickData& tick) {
    StrategyBase::onTick(symbol, tick);

    if (!running_) return;
    
    // 可以在这里添加基于Tick数据的交易逻辑
    // 例如更精细的止损/止盈判断等
}

void MomentumStrategy::onSnapshot(const Snapshot& snapshot) {
    StrategyBase::onSnapshot(snapshot);

    if (!running_) return;
    
    // 更新持仓的市价
    auto& pos_mgr = PositionManager::getInstance();
    pos_mgr.updateMarketPrice(snapshot.symbol, snapshot.last_price);
}

bool MomentumStrategy::shouldEnter(const ScanResult& result, const std::vector<KlineData>& klines) {
    // 入场条件
    
    // 1. 必须是上升趋势
    if (!isUptrend(klines)) {
        return false;
    }
    
    // 2. RSI不能过高（避免追高）
    double rsi = calculateRSI(klines);
    if (rsi > 70.0 || rsi < 30.0) {
        return false;
    }
    
    // 3. 涨幅合理（2%-6%之间）
    if (result.change_ratio < 0.02 || result.change_ratio > 0.06) {
        return false;
    }
    
    // 4. 有足够的成交量
    if (result.turnover_rate < 0.02) {
        return false;
    }
    
    return true;
}

bool MomentumStrategy::shouldExit(const std::string& symbol, double current_price) {
    auto& risk_mgr = RiskManager::getInstance();
    
    // 止损或止盈
    if (risk_mgr.shouldStopLoss(symbol, current_price) ||
        risk_mgr.shouldTakeProfit(symbol, current_price)) {
        return true;
    }
    
    return false;
}

int MomentumStrategy::calculateQuantity(const std::string& symbol, double price) {
    (void)symbol;
    (void)price;
    
    auto& risk_mgr = RiskManager::getInstance();
    const auto& config = ConfigManager::getInstance().getConfig();
    
    // 假设有足够的资金
    double available_cash = config.trading.max_position_size * 0.3;  // 使用30%的资金
    
    return risk_mgr.calculatePositionSize(price, available_cash);
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

