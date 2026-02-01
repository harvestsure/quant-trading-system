#include "scanner/market_scanner.h"
#include "managers/strategy_manager.h"
#include "config/config_manager.h"
#include "utils/logger.h"
#include <chrono>
#include <thread>
#include <algorithm>
#include <sstream>

// 注意：这里需要包含Futu API的头文件
// #include "ftdc_trader_api.h"

MarketScanner::MarketScanner() : running_(false) {
    LOG_INFO("Market scanner initialized");
}

MarketScanner::~MarketScanner() {
    stop();
}

void MarketScanner::start() {
    if (running_) {
        LOG_WARNING("Market scanner already running");
        return;
    }
    
    running_ = true;
    scan_thread_ = std::make_unique<std::thread>(&MarketScanner::scanLoop, this);
    
    LOG_INFO("Market scanner started");
}

void MarketScanner::stop() {
    if (!running_) return;
    
    running_ = false;
    if (scan_thread_ && scan_thread_->joinable()) {
        scan_thread_->join();
    }
    
    LOG_INFO("Market scanner stopped");
}

void MarketScanner::scanLoop() {
    const auto& config = ConfigManager::getInstance().getConfig();
    auto scan_interval = std::chrono::minutes(config.scanner.interval_minutes);
    
    while (running_) {
        try {
            scanOnce();
        } catch (const std::exception& e) {
            std::stringstream ss;
            ss << "Scan error: " << e.what();
            LOG_ERROR(ss.str());
        }
        
        // 等待下一次扫描
        auto next_scan_time = std::chrono::steady_clock::now() + scan_interval;
        while (running_ && std::chrono::steady_clock::now() < next_scan_time) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void MarketScanner::scanOnce() {
    LOG_INFO("Starting market scan...");
    
    auto results = performScan();
    
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
    
    // 只保留前10名
    if (filtered_results.size() > 10) {
        filtered_results.resize(10);
    }
    
    std::stringstream ss;
    ss << "Scan completed: found " << filtered_results.size() << " qualified stocks";
    LOG_INFO(ss.str());
    
    // 将结果传递给策略管理器
    if (!filtered_results.empty()) {
        StrategyManager::getInstance().processScanResults(filtered_results);
    }
}

std::vector<ScanResult> MarketScanner::performScan() {
    std::vector<ScanResult> results;
    
    // TODO: 这里需要调用Futu API获取市场数据
    // 示例代码结构：
    /*
    FTDC_Trader_API* api = FTDC_Trader_API::getInstance();
    
    // 获取港股列表
    std::vector<std::string> stock_list;
    api->getStockList("HK", stock_list);
    
    // 获取每只股票的行情数据
    for (const auto& symbol : stock_list) {
        ScanResult result;
        result.symbol = symbol;
        
        // 获取快照数据
        api->getSnapshot(symbol, result);
        
        // 获取历史数据计算指标
        std::vector<KLine> klines;
        api->getHistoryKLine(symbol, "K_5M", 100, klines);
        
        // 计算评分
        result.score = calculateScore(result);
        
        results.push_back(result);
    }
    */
    
    // 模拟数据用于测试
    ScanResult dummy;
    dummy.symbol = "HK.00700";
    dummy.stock_name = "腾讯控股";
    dummy.price = 350.0;
    dummy.change_ratio = 0.025;  // 2.5%涨幅
    dummy.volume = 10000000;
    dummy.turnover_rate = 0.05;
    dummy.score = calculateScore(dummy);
    results.push_back(dummy);
    
    return results;
}

bool MarketScanner::meetsSelectionCriteria(const ScanResult& result) {
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

double MarketScanner::calculateScore(const ScanResult& result) {
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

