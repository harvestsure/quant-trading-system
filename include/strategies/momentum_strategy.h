#pragma once

#include "strategies/strategy_base.h"
#include <map>
#include <vector>
#include <chrono>
#include <mutex>

// Momentum strategy (chase breakouts) - detect breakout stocks and chase
// with real-time dynamic trailing stop and take-profit
class MomentumStrategy : public StrategyBase {
public:
    MomentumStrategy();
    ~MomentumStrategy() override = default;
    
    void onScanResult(const ScanResult& result) override;
    void onKLine(const std::string& symbol, const KlineData& kline) override;
    void onTick(const std::string& symbol, const TickData& tick) override;
    void onSnapshot(const Snapshot& snapshot) override;
    
private:
    // Chase tracking information
    struct ChaseEntry {
        double entry_price = 0.0;          // entry price
        double high_water_mark = 0.0;      // highest price since entry (for trailing profit)
        double entry_volume_ratio = 0.0;   // volume ratio at entry
        double entry_score = 0.0;          // breakout score at entry
        int64_t entry_time_ms = 0;         // entry timestamp
    };

    std::map<std::string, ChaseEntry> chase_entries_;  // tracking of chased positions
    mutable std::mutex chase_mutex_;
    
    // Technical indicator calculations
    double calculateRSI(const std::vector<KlineData>& klines, int period = 14);
    double calculateMACD(const std::vector<KlineData>& klines);
    bool isUptrend(const std::vector<KlineData>& klines);
    
    // Entry/exit decision logic for momentum chasing
    bool shouldEnter(const ScanResult& result, const std::vector<KlineData>& klines);
    bool shouldChaseExit(const std::string& symbol, double current_price, double speed);
    
    // Position sizing
    int calculateQuantity(const std::string& symbol, double price);
    
    // Utility methods
    int64_t currentTimeMs() const;
};

 
