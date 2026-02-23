#pragma once

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <set>
#include <mutex>
#include "common/object.h"
#include "event/event_interface.h"
#include "exchange/exchange_interface.h"

 

class StrategyBase;

struct ScanResult {
    std::string symbol;
    std::string stock_name;
    double price;
    double change_ratio;
    double volume;
    double turnover_rate;
    double score;  // score
    std::string exchange_name;  // exchange name
    std::shared_ptr<IExchange> exchange;  // exchange instance

    // === Breakout detection metrics ===
    double volume_ratio = 0.0;      // Volume ratio: current volume / historical average volume
    double amplitude = 0.0;         // Amplitude: (high - low) / open
    double speed = 0.0;             // Speed: recent minutes' price change rate
    double bid_ask_ratio = 0.0;     // Bid-ask ratio: buy strength / sell strength
    double open_price = 0.0;        // open price
    double high_price = 0.0;        // high price
    double low_price = 0.0;         // low price
    double pre_close = 0.0;         // previous close
    double price_vs_high = 0.0;     // price distance to high: (high - price) / high
};

// Strategy instance information
struct StrategyInstance {
    std::string symbol;
    std::shared_ptr<StrategyBase> strategy;
    bool is_active;
    std::string exchange_name;  // exchange name
    std::shared_ptr<IExchange> exchange;  // corresponding exchange instance
};

class StrategyManager {
public:
    static StrategyManager& getInstance();
    
    // Initialize event handlers
    void initializeEventHandlers(IEventEngine* event_engine);

    // Dynamic strategy management - create/remove strategy instances based on scan results
    void processScanResults(const std::vector<ScanResult>& results);

    // Strategy instance management
    void createStrategyInstance(const std::string& symbol, const ScanResult& scan_result);
    void removeStrategyInstance(const std::string& symbol, bool force = false);
    bool hasStrategyInstance(const std::string& symbol) const;

    // Start/stop all strategies
    void startAllStrategies();
    void stopAllStrategies();

    // Strategy status
    size_t getActiveStrategyCount() const;
    std::vector<std::string> getStrategyStockCodes() const;
    void printStrategyStatus() const;
    
    // Non-copyable
    StrategyManager(const StrategyManager&) = delete;
    StrategyManager& operator=(const StrategyManager&) = delete;
    
private:
    StrategyManager() = default;
    
    // stock symbol -> strategy instance
    std::map<std::string, StrategyInstance> strategy_instances_;

    // set of stock symbols from last scan
    std::set<std::string> last_scan_stocks_;

    // event engine pointer
    IEventEngine* event_engine_ = nullptr;

    // event handler IDs
    int kline_handler_id_ = -1;
    int tick_handler_id_ = -1;
    int trade_handler_id_ = -1;
    
    mutable std::mutex mutex_;
    
    // internal helper functions
    bool canRemoveStrategy(const std::string& symbol) const;
    std::shared_ptr<StrategyBase> createStrategy(const std::string& symbol, const ScanResult& scan_result);

    // event handlers
    void onKLineEvent(const EventPtr& event);
    void onTickEvent(const EventPtr& event);
    void onTradeEvent(const EventPtr& event);
};

 
