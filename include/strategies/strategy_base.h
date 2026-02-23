#pragma once

#include "managers/strategy_manager.h"
#include "data/data_subscriber.h"
#include <string>
#include <atomic>
#include <map>

 

class StrategyBase {
public:
    explicit StrategyBase(const std::string& name);
    virtual ~StrategyBase() = default;
    
    // Strategy lifecycle
    virtual void start();
    virtual void stop();
    bool isRunning() const { return running_; }
    
    // Strategy name
    std::string getName() const { return name_; }
    
    // Handle scan results
    virtual void onScanResult(const ScanResult& result) = 0;
    
    // Data callbacks (subclasses may override selectively)
    virtual void onKLine(const std::string& symbol, const KlineData& kline);
    virtual void onTick(const std::string& symbol, const TickData& tick);
    virtual void onSnapshot(const Snapshot& snapshot);
    
protected:
    std::string name_;
    std::atomic<bool> running_;
    
    // Subscribed stock list
    std::map<std::string, bool> subscribed_stocks_;
    
    // Helper methods
    void subscribeStock(const std::string& symbol);
    void unsubscribeStock(const std::string& symbol);
    
    // Trading methods
    bool buy(const std::string& symbol, int quantity, double price = 0.0);
    bool sell(const std::string& symbol, int quantity, double price = 0.0);
    
    // Get historical data
    std::vector<KlineData> getHistoryKLine(
        const std::string& symbol,
        const std::string& kline_type,
        int count
    );
};

 
