#include "scanner/market_scanner.h"
#include "managers/strategy_manager.h"
#include "config/config_manager.h"
#include "utils/logger.h"
#include <chrono>
#include <thread>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <cmath>
#include <sstream>

MarketScanner::MarketScanner() : running_(false) {
    LOG_INFO("Market scanner initialized");
}

MarketScanner::~MarketScanner() {
    stop();
}

void MarketScanner::addExchange(std::shared_ptr<IExchange> exchange) {
    std::lock_guard<std::mutex> lock(exchanges_mutex_);
    if (exchange) {
        exchanges_.push_back(exchange);
        LOG_INFO("Exchange added: " + exchange->getName());
    }
}

void MarketScanner::start() {
    if (running_) {
        LOG_WARN("Market scanner already running");
        return;
    }
    
    // 从配置文件加载扫描参数
    scanner_params_ = ConfigManager::getInstance().getScannerParams();
    LOG_INFO("Loaded scanner config - top_n: " + std::to_string(scanner_params_.top_n) +
             ", breakout_vol_ratio: " + std::to_string(scanner_params_.breakout_volume_ratio_min) +
             ", breakout_change: [" + std::to_string(scanner_params_.breakout_change_ratio_min) + 
             ", " + std::to_string(scanner_params_.breakout_change_ratio_max) + "]");
    
    std::lock_guard<std::mutex> lock(exchanges_mutex_);
    if (exchanges_.empty()) {
        LOG_ERROR("No exchanges configured");
        return;
    }
    
    running_ = true;
    scan_thread_ = std::make_unique<std::thread>(&MarketScanner::scanLoop, this);
    
    LOG_INFO("Market scanner started with " + std::to_string(exchanges_.size()) + " exchange(s)");
}

void MarketScanner::stop() {
    if (!running_) return;
    
    running_ = false;
    if (scan_thread_ && scan_thread_->joinable()) {
        scan_thread_->join();
    }
    
    LOG_INFO("Market scanner stopped");
}

void MarketScanner::setWatchList(const std::string& exchange_name, const std::vector<std::string>& watch_list) {
    std::lock_guard<std::mutex> lock(watch_list_mutex_);
    watch_lists_[exchange_name] = watch_list;
    
    LOG_INFO("Watch list set for " + exchange_name + ": " + std::to_string(watch_list.size()) + " stocks");
}

void MarketScanner::clearWatchLists() {
    std::lock_guard<std::mutex> lock(watch_list_mutex_);
    watch_lists_.clear();
    LOG_INFO("All watch lists cleared");
}

MarketScanner::ScannerStatus MarketScanner::getStatus() const {
    std::lock_guard<std::mutex> watch_lock(watch_list_mutex_);
    std::lock_guard<std::mutex> qualified_lock(qualified_stocks_mutex_);
    std::lock_guard<std::mutex> exch_lock(exchanges_mutex_);
    
    std::vector<std::string> active_exchanges;
    std::map<std::string, int> watch_counts;
    
    for (const auto& exch : exchanges_) {
        if (exch && exch->isConnected()) {
            active_exchanges.push_back(exch->getName());
            auto it = watch_lists_.find(exch->getName());
            if (it != watch_lists_.end()) {
                watch_counts[exch->getName()] = it->second.size();
            }
        }
    }
    
    return {
        running_,
        watch_counts,
        qualified_stocks_,
        isInTradingTime(),
        isInOpeningPeriod(),
        active_exchanges
    };
}

void MarketScanner::scanLoop() {
    // 初始化各个交易所的监控列表（从对应的交易所获取）
    {
        std::lock_guard<std::mutex> lock(watch_list_mutex_);
        std::lock_guard<std::mutex> exch_lock(exchanges_mutex_);
        
        for (const auto& exchange : exchanges_) {
            if (!exchange || !exchange->isConnected()) {
                continue;
            }
            
            const auto& exch_name = exchange->getName();
            if (watch_lists_.find(exch_name) == watch_lists_.end()) {
                auto stock_list = exchange->getMarketStockList();
                if (!stock_list.empty()) {
                    watch_lists_[exch_name] = stock_list;
                    LOG_INFO("Loaded " + std::to_string(stock_list.size()) + " stocks from " + exch_name);
                    // 量比历史数据采用懒加载策略，在首次扫描时按需获取
                    LOG_INFO("Volume history will be loaded on-demand during scanning");
                }
            }
        }
    }
    
    while (running_) {
        try {
            if (/*isInTradingTime()*/ true) {
                // 为每个交易所执行扫描
                std::lock_guard<std::mutex> exch_lock(exchanges_mutex_);
                for (const auto& exchange : exchanges_) {
                    if (exchange && exchange->isConnected()) {
                        performScan(exchange);
                    }
                }
                
                // 根据时段选择扫描间隔
                int interval_ms = isInOpeningPeriod() ? 
                    OPENING_SCAN_INTERVAL_MS : NORMAL_SCAN_INTERVAL_MS;
                
                // 分次等待，以便快速响应stop()
                auto end_time = std::chrono::steady_clock::now() + 
                               std::chrono::milliseconds(interval_ms);
                while (running_ && std::chrono::steady_clock::now() < end_time) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } else {
                // 非交易时段，低频检查
                std::this_thread::sleep_for(std::chrono::milliseconds(NON_TRADING_SCAN_INTERVAL_MS));
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("Scan loop error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
}

void MarketScanner::performScan(const std::shared_ptr<IExchange>& exchange) {
    if (!exchange) {
        return;
    }
    
    const auto& exch_name = exchange->getName();
    std::vector<std::string> watch_list;
    
    // 获取该交易所的监控列表
    {
        std::lock_guard<std::mutex> lock(watch_list_mutex_);
        auto it = watch_lists_.find(exch_name);
        if (it == watch_lists_.end() || it->second.empty()) {
            LOG_WARN("No watch list for exchange: " + exch_name);
            return;
        }
        watch_list = it->second;
    }
    
    LOG_INFO("Starting breakout scan for " + exch_name + " (" + std::to_string(watch_list.size()) + " stocks)...");
    
    // 获取市场数据
    auto results = batchFetchMarketData(exchange, watch_list);
    
    // 筛选符合爆发条件的股票
    std::vector<ScanResult> filtered_results;
    for (auto& result : results) {
        if (meetsSelectionCriteria(result)) {
            filtered_results.push_back(result);
        }
    }
    
    // 按爆发评分排序
    std::sort(filtered_results.begin(), filtered_results.end(),
        [](const ScanResult& a, const ScanResult& b) {
            return a.score > b.score;
        });
    
    // 只保留前top_n名
    if (filtered_results.size() > static_cast<size_t>(scanner_params_.top_n)) {
        filtered_results.resize(scanner_params_.top_n);
    }
    
    // 打印爆发股详情
    if (!filtered_results.empty()) {
        std::stringstream ss;
        ss << "\n=== Breakout Scan Results (" << exch_name << ") ===";
        for (size_t i = 0; i < filtered_results.size(); ++i) {
            const auto& r = filtered_results[i];
            ss << "\n  #" << (i + 1) << " " << r.symbol << " " << r.stock_name
               << " | Price: " << r.price
               << " | Chg: " << std::fixed << std::setprecision(2) << (r.change_ratio * 100) << "%"
               << " | VolRatio: " << std::setprecision(1) << r.volume_ratio << "x"
               << " | Amp: " << std::setprecision(2) << (r.amplitude * 100) << "%"
               << " | Speed: " << std::setprecision(2) << (r.speed * 100) << "%"
               << " | Turnover: " << std::setprecision(2) << (r.turnover_rate * 100) << "%"
               << " | B/A: " << std::setprecision(2) << r.bid_ask_ratio
               << " | vsHigh: " << std::setprecision(2) << (r.price_vs_high * 100) << "%"
               << " | Score: " << std::setprecision(1) << r.score;
        }
        LOG_INFO(ss.str());
    }
    
    // 更新合格股票列表
    {
        std::lock_guard<std::mutex> lock(qualified_stocks_mutex_);
        qualified_stocks_[exch_name].clear();
        for (const auto& result : filtered_results) {
            qualified_stocks_[exch_name].push_back(result.symbol);
        }
    }
    
    LOG_INFO("Scan completed for " + exch_name + ": found " + std::to_string(filtered_results.size()) + " breakout stocks");
    
    // 将结果传递给策略管理器
    if (!filtered_results.empty()) {
        StrategyManager::getInstance().processScanResults(filtered_results);
    }
}

std::vector<ScanResult> MarketScanner::batchFetchMarketData(const std::shared_ptr<IExchange>& exchange, const std::vector<std::string>& symbols) {
    std::vector<ScanResult> all_results;
    
    if (!exchange || !exchange->isConnected()) {
        LOG_ERROR("Exchange not connected");
        return all_results;
    }
    
    // 分批获取数据
    for (size_t i = 0; i < symbols.size(); i += BATCH_SIZE) {
        if (!running_) {
            break;
        }
        
        size_t end_idx = std::min(i + BATCH_SIZE, symbols.size());
        std::vector<std::string> batch(symbols.begin() + i, symbols.begin() + end_idx);
        
        try {
            // 调用交易所的批量快照接口
            auto snapshots = exchange->getBatchSnapshots(batch);
            
            // 转换为ScanResult并计算爆发指标和评分
            for (const auto& pair : snapshots) {
                ScanResult result = convertSnapshotToScanResult(pair.second, exchange->getName(), exchange);
                result.score = calculateScore(result);
                all_results.push_back(result);
                
                // 更新历史数据（用于下次计算涨速）
                updateVolumeHistory(result.symbol, pair.second.volume, pair.second.last_price);
                
                // 缓存本次快照（用于计算涨速）
                {
                    std::lock_guard<std::mutex> lock(last_snapshots_mutex_);
                    last_snapshots_[result.symbol] = pair.second;
                }
            }
            
            // 批次间隔，避免过快请求
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to fetch batch [" + std::to_string(i) + ", " + std::to_string(end_idx) + "): " + std::string(e.what()));
        }
    }
    
    return all_results;
}

ScanResult MarketScanner::convertSnapshotToScanResult(const Snapshot& snapshot, 
                                                       const std::string& exchange_name,
                                                       std::shared_ptr<IExchange> exchange) {
    ScanResult result;
    result.symbol = snapshot.symbol;
    result.stock_name = snapshot.name;
    result.price = snapshot.last_price;
    result.change_ratio = (snapshot.pre_close > 0) ? 
        (snapshot.last_price - snapshot.pre_close) / snapshot.pre_close : 0.0;
    result.volume = snapshot.volume;
    result.turnover_rate = snapshot.turnover_rate;
    result.exchange_name = exchange_name;
    result.exchange = exchange;
    
    // === 爆发检测指标计算 ===
    
    // 1. 量比 - 延迟加载，仅对通过基本筛选的股票计算（避免对3583只股全部请求K线）
    result.volume_ratio = -1.0;  // 标记为未计算
    
    // 2. 振幅（日内波动幅度）
    if (snapshot.open_price > 0) {
        result.amplitude = (snapshot.high_price - snapshot.low_price) / snapshot.open_price;
    }
    
    // 3. 涨速（与上次扫描相比的价格变化速度）
    result.speed = calculateSpeed(snapshot.symbol, snapshot.last_price);
    
    // 4. 委买委卖比
    result.bid_ask_ratio = calculateBidAskRatio(snapshot);
    
    // 5. 价格信息
    result.open_price = snapshot.open_price;
    result.high_price = snapshot.high_price;
    result.low_price = snapshot.low_price;
    result.pre_close = snapshot.pre_close;
    
    // 6. 距离最高价的比例（越小说明越接近新高，主力正在拉升）
    if (snapshot.high_price > 0) {
        result.price_vs_high = (snapshot.high_price - snapshot.last_price) / snapshot.high_price;
    }
    
    return result;
}

std::pair<int, int> MarketScanner::getCurrentTime() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    return {tm.tm_hour, tm.tm_min};
}

bool MarketScanner::isInTradingTime() const {
    auto time_pair = getCurrentTime();
    int hour = time_pair.first;
    int minute = time_pair.second;
    int current_min = hour * 60 + minute;
    
    // 香港股市：9:30-12:00（上午）, 13:00-16:00（下午）
    int morning_open = 9 * 60 + 30;    // 9:30
    int morning_close = 12 * 60;       // 12:00
    int afternoon_open = 13 * 60;      // 13:00
    int afternoon_close = 16 * 60;     // 16:00
    
    bool is_morning = current_min >= morning_open && current_min <= morning_close;
    bool is_afternoon = current_min >= afternoon_open && current_min <= afternoon_close;
    
    return is_morning || is_afternoon;
}

bool MarketScanner::isInOpeningPeriod() const {
    auto time_pair = getCurrentTime();
    int hour = time_pair.first;
    int minute = time_pair.second;
    
    // 开盘后30分钟（9:30-10:00）和下午开盘后（13:00-13:30）
    // 这两个时段是爆发股最容易出现的关键窗口
    bool morning_opening = (hour == 9 && minute >= 30) || (hour == 10 && minute == 0);
    bool afternoon_opening = (hour == 13 && minute < 30);
    return morning_opening || afternoon_opening;
}

bool MarketScanner::meetsSelectionCriteria(ScanResult& result) {
    // ===== 港股爆发股筛选条件 =====
    // 核心思路：找到成交量突然放大、价格快速拉升的个股
    
    // 1. 价格过滤（避免仙股和超大盘股）
    if (result.price < scanner_params_.min_price || result.price > scanner_params_.max_price) {
        return false;
    }
    
    // 2. 涨幅范围：必须正向上涨
    //    太低(<2%) = 不够强势，太高(>10%) = 追高风险大
    if (result.change_ratio < scanner_params_.breakout_change_ratio_min || 
        result.change_ratio > scanner_params_.breakout_change_ratio_max) {
        return false;
    }
    
    // 3. 量比条件：倸带加载 - 仅对通过以上基本筛选的股票计算量比
    //    成交量异常放大是爆发的核心信号
    if (result.volume_ratio < 0) {
        // 延迟加载量比，使用 result 中的 exchange 指针避免重新加锁
        result.volume_ratio = calculateVolumeRatio(result.symbol, result.volume, result.exchange);
    }
    if (result.volume_ratio < scanner_params_.breakout_volume_ratio_min) {
        return false;
    }
    
    // 4. 振幅条件：有波动才有操作空间
    if (result.amplitude < scanner_params_.breakout_amplitude_min) {
        return false;
    }
    
    // 5. 换手率：市场关注度和流动性
    if (result.turnover_rate < scanner_params_.min_turnover_rate) {
        return false;
    }
    
    // 6. 绝对成交量过滤
    if (result.volume < scanner_params_.min_volume) {
        return false;
    }
    
    // 7. 买卖力量：买盘应强于卖盘
    if (result.bid_ask_ratio < 0.8) {
        return false;  // 卖压太大，不适合追涨
    }
    
    // 8. 距离最高价：不追已经大幅回落的股票
    //    价格在日内最高价附近 => 主力还在拉升
    if (result.price_vs_high > 0.05) {
        return false;  // 距最高价超5%，可能已回落
    }
    
    return true;
}

double MarketScanner::calculateScore(const ScanResult& result) const {
    // ===== 港股爆发股综合评分算法 =====
    // 每个维度归一化到 [0, 1]，然后加权求和
    double score = 0.0;
    
    // 1. 量比评分（权重35%）：量比越大越好，超过10x后边际递减
    double volume_score = std::min(1.0, result.volume_ratio / 10.0);
    score += volume_score * scanner_params_.breakout_score_weight_volume;
    
    // 2. 涨幅评分（权重25%）：3-6% 是甘蜜区间
    double change_score = 0.0;
    if (result.change_ratio >= 0.03 && result.change_ratio <= 0.06) {
        change_score = 1.0;  // 甘蜜区间
    } else if (result.change_ratio > 0.06) {
        change_score = std::max(0.0, 1.0 - (result.change_ratio - 0.06) / 0.04);
    } else {
        change_score = result.change_ratio / 0.03;
    }
    score += change_score * scanner_params_.breakout_score_weight_change;
    
    // 3. 涨速评分（权重25%）：涨速越快势头越猛
    double speed_score = std::min(1.0, std::max(0.0, result.speed * 100.0));
    score += speed_score * scanner_params_.breakout_score_weight_speed;
    
    // 4. 换手率评分（权重15%）
    double turnover_score = std::min(1.0, result.turnover_rate / 0.10);
    score += turnover_score * scanner_params_.breakout_score_weight_turnover;
    
    // 5. 加分：买盘压倒性优势
    if (result.bid_ask_ratio > 2.0) {
        score += 5.0;
    }
    
    // 6. 加分：接近日内新高（主力正在拉升）
    if (result.price_vs_high < 0.01) {
        score += 5.0;
    }
    
    // 7. 开盘时段加分（开盘爆发更有持续性）
    if (isInOpeningPeriod()) {
        score *= 1.1;
    }
    
    return score;
}

// ===== 爆发检测核心方法 =====

double MarketScanner::calculateVolumeRatio(const std::string& symbol, int64_t current_volume, 
                                            const std::shared_ptr<IExchange>& exchange) {
    // 检查是否已有历史数据，没有则懒加载
    bool need_load = false;
    {
        std::lock_guard<std::mutex> lock(volume_history_mutex_);
        auto it = volume_history_.find(symbol);
        if (it == volume_history_.end() || it->second.avg_volume <= 0) {
            need_load = true;
        }
    }
    
    if (need_load && exchange && exchange->isConnected()) {
        // 懒加载：首次遇到该股票时获取历史日K线
        try {
            auto klines = exchange->getHistoryKLine(symbol, "K_DAY", VOLUME_HISTORY_DAYS + 1);
            if (klines.size() >= 2) {
                std::lock_guard<std::mutex> lock(volume_history_mutex_);
                auto& history = volume_history_[symbol];
                history.daily_volumes.clear();
                for (size_t j = 0; j < klines.size() - 1; ++j) {
                    history.daily_volumes.push_back(klines[j].volume);
                }
                int64_t total = 0;
                for (auto v : history.daily_volumes) total += v;
                history.avg_volume = total / (int64_t)history.daily_volumes.size();
                history.last_price = klines.back().close_price;
            }
        } catch (...) {}
    }
    
    // 计算量比
    std::lock_guard<std::mutex> lock(volume_history_mutex_);
    auto it = volume_history_.find(symbol);
    if (it == volume_history_.end() || it->second.avg_volume <= 0) {
        return 1.0;  // 无历史数据，默认量比为1
    }
    
    // 根据当日已过交易时间对成交量进行估算
    // 港股交易时间 9:30-12:00 + 13:00-16:00 共330分钟
    auto time_pair = getCurrentTime();
    int current_min = time_pair.first * 60 + time_pair.second;
    
    int elapsed_minutes = 0;
    int morning_open = 9 * 60 + 30;
    int morning_close = 12 * 60;
    int afternoon_open = 13 * 60;
    
    if (current_min <= morning_close) {
        elapsed_minutes = std::max(1, current_min - morning_open);
    } else if (current_min < afternoon_open) {
        elapsed_minutes = morning_close - morning_open;  // 150分钟
    } else {
        elapsed_minutes = (morning_close - morning_open) + std::max(1, current_min - afternoon_open);
    }
    
    // 按已过时间外推估算全天成交量
    double total_trading_minutes = 330.0;
    double estimated_daily_volume = (double)current_volume * total_trading_minutes / elapsed_minutes;
    
    return estimated_daily_volume / it->second.avg_volume;
}

double MarketScanner::calculateSpeed(const std::string& symbol, double current_price) const {
    std::lock_guard<std::mutex> lock(last_snapshots_mutex_);
    
    auto it = last_snapshots_.find(symbol);
    if (it == last_snapshots_.end() || it->second.last_price <= 0) {
        return 0.0;
    }
    
    // 涨速 = (当前价 - 上次价) / 上次价
    return (current_price - it->second.last_price) / it->second.last_price;
}

double MarketScanner::calculateBidAskRatio(const Snapshot& snapshot) const {
    if (snapshot.ask_volume_1 <= 0) {
        return (snapshot.bid_volume_1 > 0) ? 10.0 : 1.0;
    }
    return (double)snapshot.bid_volume_1 / snapshot.ask_volume_1;
}

void MarketScanner::updateVolumeHistory(const std::string& symbol, int64_t volume, double price) {
    std::lock_guard<std::mutex> lock(volume_history_mutex_);
    auto& history = volume_history_[symbol];
    history.last_price = price;
    history.last_scan_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void MarketScanner::initVolumeHistory(
    const std::shared_ptr<IExchange>& exchange, 
    const std::vector<std::string>& symbols) {
    
    if (!exchange || !exchange->isConnected()) return;
    
    LOG_INFO("Initializing volume history for breakout detection (" + 
             std::to_string(symbols.size()) + " symbols)...");
    
    int count = 0;
    
    for (const auto& symbol : symbols) {
        if (!running_) break;
        
        try {
            // 获取近N日的日K线数据
            auto klines = exchange->getHistoryKLine(symbol, "K_DAY", VOLUME_HISTORY_DAYS + 1);
            
            if (klines.size() >= 2) {
                std::lock_guard<std::mutex> lock(volume_history_mutex_);
                auto& history = volume_history_[symbol];
                history.daily_volumes.clear();
                
                // 跳过当天数据（最后一条），使用之前的历史数据
                for (size_t j = 0; j < klines.size() - 1; ++j) {
                    history.daily_volumes.push_back(klines[j].volume);
                }
                
                // 计算平均成交量
                if (!history.daily_volumes.empty()) {
                    int64_t total = 0;
                    for (auto v : history.daily_volumes) total += v;
                    history.avg_volume = total / (int64_t)history.daily_volumes.size();
                }
                
                history.last_price = klines.back().close_price;
                count++;
            }
        } catch (const std::exception& e) {
            // 静默跳过失败的个股
            (void)e;
        }
        
        if (count % 50 == 0 && count > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
    
    LOG_INFO("Volume history initialized for " + std::to_string(count) + " stocks");
}

