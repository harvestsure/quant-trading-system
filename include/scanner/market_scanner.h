#pragma once

#include "exchange/exchange_interface.h"
#include "managers/strategy_manager.h"
#include "config/config_manager.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <chrono>
#include <ctime>
#include <mutex>
#include <deque>

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
        std::map<std::string, int> watch_list_counts;
        std::map<std::string, std::vector<std::string>> qualified_stocks;
        bool is_trading_time;
        bool is_opening_period;
        std::vector<std::string> active_exchanges;
    };
    
    ScannerStatus getStatus() const;
    
private:
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> scan_thread_;
    std::vector<std::shared_ptr<IExchange>> exchanges_;
    mutable std::mutex exchanges_mutex_;
    
    // 监控列表（按交易所分类）
    std::map<std::string, std::vector<std::string>> watch_lists_;
    mutable std::mutex watch_list_mutex_;
    
    // 最近一次的合格股票（按交易所分类）
    std::map<std::string, std::vector<std::string>> qualified_stocks_;
    mutable std::mutex qualified_stocks_mutex_;
    
    // 扫描参数配置
    ScannerParams scanner_params_;
    static constexpr int BATCH_SIZE = 400;
    static constexpr int OPENING_SCAN_INTERVAL_MS = 30000;     // 开盘期间30秒（更快捕捉爆发）
    static constexpr int NORMAL_SCAN_INTERVAL_MS = 60000;      // 正常时段60秒
    static constexpr int NON_TRADING_SCAN_INTERVAL_MS = 120000; // 非交易时段120秒
    
    // === 爆发检测相关 ===
    // 历史成交量缓存（用于计算量比）
    struct VolumeHistory {
        std::deque<int64_t> daily_volumes;  // 近N日成交量
        int64_t avg_volume = 0;             // 平均成交量
        double last_price = 0.0;            // 上次扫描时的价格（用于计算涨速）
        int64_t last_scan_time = 0;         // 上次扫描时间戳
    };
    std::map<std::string, VolumeHistory> volume_history_;
    mutable std::mutex volume_history_mutex_;
    static constexpr int VOLUME_HISTORY_DAYS = 5;
    
    // 上次扫描的快照缓存（用于计算涨速）
    std::map<std::string, Snapshot> last_snapshots_;
    mutable std::mutex last_snapshots_mutex_;
    
    void scanLoop();
    void performScan(const std::shared_ptr<IExchange>& exchange);
    
    // 分批获取市场快照
    std::vector<ScanResult> batchFetchMarketData(const std::shared_ptr<IExchange>& exchange, const std::vector<std::string>& symbols);
    
    // 时间检查
    bool isInTradingTime() const;
    bool isInOpeningPeriod() const;
    
    // 筛选和评分
    bool meetsSelectionCriteria(ScanResult& result);
    double calculateScore(const ScanResult& result) const;
    ScanResult convertSnapshotToScanResult(const Snapshot& snapshot, 
                                           const std::string& exchange_name,
                                           std::shared_ptr<IExchange> exchange = nullptr);
    
    // === 爆发检测方法 ===
    double calculateVolumeRatio(const std::string& symbol, int64_t current_volume,
                                 const std::shared_ptr<IExchange>& exchange);
    double calculateSpeed(const std::string& symbol, double current_price) const;
    double calculateBidAskRatio(const Snapshot& snapshot) const;
    void updateVolumeHistory(const std::string& symbol, int64_t volume, double price);
    void initVolumeHistory(const std::shared_ptr<IExchange>& exchange, const std::vector<std::string>& symbols);
    
    // 获取当前时刻（时:分）
    std::pair<int, int> getCurrentTime() const;
};
