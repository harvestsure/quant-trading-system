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
    
    // 添加交易所实例（支持多交易所）
    void addExchange(std::shared_ptr<IExchange> exchange);
    
    // 扫描所有已添加的交易所
    void start();
    void stop();
    bool isRunning() const { return running_; }
    
    // 设置监控列表（可选，如果为空则使用交易所的全部股票）
    void setWatchList(const std::string& exchange_name, const std::vector<std::string>& watch_list);
    void clearWatchLists();
    
    // 获取当前扫描状态
    struct ScannerStatus {
        bool running;
        std::map<std::string, int> watch_list_counts;  // 每个交易所的监控列表数量
        std::map<std::string, std::vector<std::string>> qualified_stocks;  // 每个交易所的合格股票
        bool is_trading_time;
        bool is_opening_period;
        std::vector<std::string> active_exchanges;  // 当前活跃的交易所
    };
    
    ScannerStatus getStatus() const;
    
private:
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> scan_thread_;
    std::vector<std::shared_ptr<IExchange>> exchanges_;  // 支持多交易所
    mutable std::mutex exchanges_mutex_;
    
    // 监控列表（按交易所分类）
    std::map<std::string, std::vector<std::string>> watch_lists_;  // key: exchange_name
    mutable std::mutex watch_list_mutex_;
    
    // 最近一次的合格股票（按交易所分类）
    std::map<std::string, std::vector<std::string>> qualified_stocks_;  // key: exchange_name
    mutable std::mutex qualified_stocks_mutex_;
    
    // 扫描参数配置
    static constexpr int BATCH_SIZE = 400;  // 每批扫描400个股票
    static constexpr int OPENING_SCAN_INTERVAL_MS = 60000;    // 开盘期间60秒
    static constexpr int NORMAL_SCAN_INTERVAL_MS = 90000;     // 正常时段90秒
    static constexpr int NON_TRADING_SCAN_INTERVAL_MS = 120000; // 非交易时段120秒
    
    void scanLoop();
    void performScan(const std::shared_ptr<IExchange>& exchange);
    
    // 分批获取市场快照（支持多交易所）
    std::vector<ScanResult> batchFetchMarketData(const std::shared_ptr<IExchange>& exchange, const std::vector<std::string>& symbols);
    
    // 时间检查
    bool isInTradingTime() const;
    bool isInOpeningPeriod() const;
    
    // 筛选和评分
    bool meetsSelectionCriteria(const ScanResult& result) const;
    double calculateScore(const ScanResult& result) const;
    ScanResult convertSnapshotToScanResult(const Snapshot& snapshot, const std::string& exchange_name = "") const;
    
    // 获取当前时刻（时:分）
    std::pair<int, int> getCurrentTime() const;
};
