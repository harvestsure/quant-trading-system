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
    
    // Add an exchange instance (supports multiple exchanges)
    void addExchange(std::shared_ptr<IExchange> exchange);
    
    // Start scanning all added exchanges
    void start();
    void stop();
    bool isRunning() const { return running_; }
    
    // Set the watch list (optional; if empty, use the exchange's full symbol list)
    void setWatchList(const std::string& exchange_name, const std::vector<std::string>& watch_list);
    void clearWatchLists();
    
    // Get current scanner status
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
    
    // Watch lists (grouped by exchange)
    std::map<std::string, std::vector<std::string>> watch_lists_;
    mutable std::mutex watch_list_mutex_;
    
    // Most recent qualified stocks (grouped by exchange)
    std::map<std::string, std::vector<std::string>> qualified_stocks_;
    mutable std::mutex qualified_stocks_mutex_;
    
    // Scanner parameter configuration
    ScannerParams scanner_params_;
    static constexpr int BATCH_SIZE = 400;
    static constexpr int OPENING_SCAN_INTERVAL_MS = 30000;     // 30s during opening period (faster to catch breakouts)
    static constexpr int NORMAL_SCAN_INTERVAL_MS = 60000;      // 60s during normal trading
    static constexpr int NON_TRADING_SCAN_INTERVAL_MS = 120000; // 120s outside trading hours
    
    // === Breakout detection ===
    // Historical volume cache (used to compute volume ratio)
    struct VolumeHistory {
        std::deque<int64_t> daily_volumes;  // recent N days' volumes
        int64_t avg_volume = 0;             // average volume
        double last_price = 0.0;            // last scanned price (used to compute price speed)
        int64_t last_scan_time = 0;         // last scan timestamp
    };
    std::map<std::string, VolumeHistory> volume_history_;
    mutable std::mutex volume_history_mutex_;
    static constexpr int VOLUME_HISTORY_DAYS = 5;
    
    // Cache of last scan snapshots (used to compute price speed)
    std::map<std::string, Snapshot> last_snapshots_;
    mutable std::mutex last_snapshots_mutex_;
    
    void scanLoop();
    void performScan(const std::shared_ptr<IExchange>& exchange);
    
    // Fetch market snapshots in batches
    std::vector<ScanResult> batchFetchMarketData(const std::shared_ptr<IExchange>& exchange, const std::vector<std::string>& symbols);
    
    // Time checks
    bool isInTradingTime() const;
    bool isInOpeningPeriod() const;
    
    // Filtering and scoring
    bool meetsSelectionCriteria(ScanResult& result);
    double calculateScore(const ScanResult& result) const;
    ScanResult convertSnapshotToScanResult(const Snapshot& snapshot, 
                                           const std::string& exchange_name,
                                           std::shared_ptr<IExchange> exchange = nullptr);
    
    // === Breakout detection methods ===
    double calculateVolumeRatio(const std::string& symbol, int64_t current_volume,
                                 const std::shared_ptr<IExchange>& exchange);
    double calculateSpeed(const std::string& symbol, double current_price) const;
    double calculateBidAskRatio(const Snapshot& snapshot) const;
    void updateVolumeHistory(const std::string& symbol, int64_t volume, double price);
    void initVolumeHistory(const std::shared_ptr<IExchange>& exchange, const std::vector<std::string>& symbols);
    
    // Get current time (hour, minute)
    std::pair<int, int> getCurrentTime() const;
};
