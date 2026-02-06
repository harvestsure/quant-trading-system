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
    LOG_INFO("Loaded scanner config - top_n: " + std::to_string(scanner_params_.top_n));
    
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
    
    LOG_INFO("Starting market scan for " + exch_name + "...");
    
    // 获取市场数据
    auto results = batchFetchMarketData(exchange, watch_list);
    
    // 筛选符合条件的股票
    std::vector<ScanResult> filtered_results;
    for (const auto& result : results) {
        if (meetsSelectionCriteria(result)) {
            filtered_results.push_back(result);
        }
    }
    
    // 按评分排序
    std::sort(filtered_results.begin(), filtered_results.end(),
        [](const ScanResult& a, const ScanResult& b) {
            return a.score > b.score;
        });
    
    // 只保留前top_n名（从配置文件读取）
    if (filtered_results.size() > static_cast<size_t>(scanner_params_.top_n)) {
        filtered_results.resize(scanner_params_.top_n);
    }
    
    // 更新合格股票列表
    {
        std::lock_guard<std::mutex> lock(qualified_stocks_mutex_);
        qualified_stocks_[exch_name].clear();
        for (const auto& result : filtered_results) {
            qualified_stocks_[exch_name].push_back(result.symbol);
        }
    }
    
    LOG_INFO("Scan completed for " + exch_name + ": found " + std::to_string(filtered_results.size()) + " qualified stocks");
    
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
            
            // 转换为ScanResult并计算评分
            for (const auto& pair : snapshots) {
                ScanResult result = convertSnapshotToScanResult(pair.second, exchange->getName(), exchange);
                result.score = calculateScore(result);
                all_results.push_back(result);
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
                                                       std::shared_ptr<IExchange> exchange) const {
    ScanResult result;
    result.symbol = snapshot.symbol;
    result.stock_name = snapshot.name;
    result.price = snapshot.last_price;
    result.change_ratio = snapshot.price_change/snapshot.last_price;
    result.volume = snapshot.volume;
    result.turnover_rate = snapshot.turnover_rate;
    result.exchange_name = exchange_name;
    result.exchange = exchange;  // 止了订阅事件，丐稀消息不需要exchange指针
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
    
    // 开盘后30分钟内（9:30-10:00）为关键追涨时段
    return (hour == 9 && minute >= 30) || (hour == 10 && minute < 60);
}

bool MarketScanner::meetsSelectionCriteria(const ScanResult& result) const {
    // 筛选条件：追涨杀跌策略
    
    // 1. 涨幅在1%到8%之间（强势但不是极端）
    if (result.change_ratio < 0.01 || result.change_ratio > 0.08) {
        return false;
    }
    
    // 2. 有足够的成交量（换手率大于1%）
    if (result.turnover_rate < 0.01) {
        return false;
    }
    
    // 3. 价格合理（5元到500元之间）
    if (result.price < 5.0 || result.price > 500.0) {
        return false;
    }
    
    return true;
}

double MarketScanner::calculateScore(const ScanResult& result) const {
    // 综合评分算法
    double score = 0.0;
    
    // 涨幅权重：40%
    score += result.change_ratio * 40.0;
    
    // 换手率权重：30%
    score += result.turnover_rate * 30.0;
    
    // 成交量权重：30%（标准化到0-1）
    double volume_score = std::min(1.0, result.volume / 100000000.0);
    score += volume_score * 30.0;
    
    return score;
}

