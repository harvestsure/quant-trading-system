#pragma once

#include "exchange/exchange_interface.h"
#include "managers/strategy_manager.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <chrono>
#include <ctime>
#include <mutex>

class MarketScanner {
public:
    MarketScanner();
    ~MarketScanner();
    
    // 设置交易所实例（所有行情和交易逻辑都通过这里获取）
    void setExchange(std::shared_ptr<IExchange> exchange);
    
    // 启动/停止扫描
    void start();
    void stop();
    bool isRunning() const { return running_; }
    
    // 立即执行一次扫描
    void scanOnce();
    
    // 设置监控列表（可选，如果为空则使用交易所的全部股票）
    void setWatchList(const std::vector<std::string>& watch_list);
    
    // 获取当前扫描状态
    struct ScannerStatus {
        bool running;
        int watch_list_count;
        std::vector<std::string> qualified_stocks;
        int qualified_count;
        bool is_trading_time;
        bool is_opening_period;
    };
    
    ScannerStatus getStatus() const;
    
private:
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> scan_thread_;
    std::shared_ptr<IExchange> exchange_;
    
    // 监控列表
    std::vector<std::string> watch_list_;
    mutable std::mutex watch_list_mutex_;
    
    // 最近一次的合格股票
    std::vector<std::string> qualified_stocks_;
    mutable std::mutex qualified_stocks_mutex_;
    
    // 扫描参数配置
    static constexpr int BATCH_SIZE = 400;  // 每批扫描400个股票
    static constexpr int OPENING_SCAN_INTERVAL_MS = 3000;    // 开盘期间3秒
    static constexpr int NORMAL_SCAN_INTERVAL_MS = 5000;     // 正常时段5秒
    static constexpr int NON_TRADING_SCAN_INTERVAL_MS = 60000; // 非交易时段60秒
    
    void scanLoop();
    std::vector<ScanResult> performScan();
    
    // 分批获取市场快照
    std::vector<ScanResult> batchFetchMarketData(const std::vector<std::string>& symbols);
    
    // 时间检查
    bool isInTradingTime() const;
    bool isInOpeningPeriod() const;
    
    // 筛选和评分
    bool meetsSelectionCriteria(const ScanResult& result) const;
    double calculateScore(const ScanResult& result) const;
    ScanResult convertSnapshotToScanResult(const Snapshot& snapshot) const;
    
    // 获取当前时刻（时:分）
    std::pair<int, int> getCurrentTime() const;
};
