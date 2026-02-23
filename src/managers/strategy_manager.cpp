#include "managers/strategy_manager.h"
#include "managers/position_manager.h"
#include "strategies/strategy_base.h"
#include "strategies/momentum_strategy.h"
#include "event/event.h"
#include "exchange/exchange_interface.h"
#include "utils/logger.h"
#include <algorithm>
#include <sstream>

StrategyManager& StrategyManager::getInstance() {
    static StrategyManager instance;
    return instance;
}

void StrategyManager::processScanResults(const std::vector<ScanResult>& results) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::stringstream ss;
    ss << "Processing scan results: " << results.size() << " stocks";
    LOG_INFO(ss.str());
    
    // 1. Build the set of stock symbols from the current scan
    std::set<std::string> current_scan_stocks;
    for (const auto& result : results) {
        current_scan_stocks.insert(result.symbol);
    }
    
    // 2. Create strategy instances for new symbols (skip if already exists)
    for (const auto& result : results) {
        if (!hasStrategyInstance(result.symbol)) {
            createStrategyInstance(result.symbol, result);
        } else {
            // Update scan result for existing strategy
            auto& instance = strategy_instances_[result.symbol];
            if (instance.strategy && instance.strategy->isRunning()) {
                instance.strategy->onScanResult(result);
            }
        }
    }
    
    // 3. Identify strategies to remove (symbols not present in the latest scan)
    std::vector<std::string> to_remove;
    for (const auto& pair : strategy_instances_) {
        const std::string& symbol = pair.first;
        
        // If the symbol is not in the current scan, consider removal
        if (current_scan_stocks.find(symbol) == current_scan_stocks.end()) {
            to_remove.push_back(symbol);
        }
    }
    
    // 4. Remove strategies that no longer meet criteria (check for positions)
    for (const auto& symbol : to_remove) {
        removeStrategyInstance(symbol, false);
    }
    
    // 5. Update the last scanned symbol set
    last_scan_stocks_ = current_scan_stocks;
    
    ss.str("");
    ss << "Strategy instances: Active=" << getActiveStrategyCount() 
       << ", Total=" << strategy_instances_.size();
    LOG_INFO(ss.str());
}

void StrategyManager::createStrategyInstance(const std::string& symbol, const ScanResult& scan_result) {
    // No lock needed; caller already holds the lock
    
    if (hasStrategyInstance(symbol)) {
        std::stringstream ss;
        ss << "Strategy instance already exists for " << symbol;
        LOG_WARN(ss.str());
        return;
    }
    
    // Create a new strategy instance
    auto strategy = createStrategy(symbol, scan_result);
    if (!strategy) {
        std::stringstream ss;
        ss << "Failed to create strategy for " << symbol;
        LOG_ERROR(ss.str());
        return;
    }
    
    // Subscribe to market data (using the provided exchange instance)
    if (scan_result.exchange && scan_result.exchange->isConnected()) {
        const std::string& exchange_name = scan_result.exchange_name;
        
        // Subscribe to KLine data (1 minute)
        if (!scan_result.exchange->subscribeKLine(symbol, "1m")) {
            LOG_WARN("Failed to subscribe KLine for " + symbol + " on " + exchange_name);
        }
        
        // Subscribe to Tick data
        if (!scan_result.exchange->subscribeTick(symbol)) {
            LOG_WARN("Failed to subscribe Tick for " + symbol + " on " + exchange_name);
        }
        
        LOG_INFO("Subscribed market data for " + symbol + " on " + exchange_name);
    } else {
        LOG_WARN("Exchange not ready for " + symbol + ", cannot subscribe market data");
    }
    
    // Start the strategy
    strategy->start();
    
    // Pass the scan result to the strategy
    strategy->onScanResult(scan_result);
    
    // Save the strategy instance
    StrategyInstance instance;
    instance.symbol = symbol;
    instance.strategy = strategy;
    instance.is_active = true;
    instance.exchange_name = scan_result.exchange_name;
    instance.exchange = scan_result.exchange;
    
    strategy_instances_[symbol] = instance;
    
    std::stringstream ss;
    ss << "Created strategy instance for " << symbol 
       << " (" << scan_result.stock_name << ")"
       << " on " << scan_result.exchange_name
       << " - Score: " << scan_result.score
       << ", Price: " << scan_result.price
       << ", Change: " << (scan_result.change_ratio * 100) << "%";
    LOG_INFO(ss.str());
}

void StrategyManager::removeStrategyInstance(const std::string& symbol, bool force) {
    // No lock needed; caller already holds the lock
    
    auto it = strategy_instances_.find(symbol);
    if (it == strategy_instances_.end()) {
        return;
    }
    
    // If not force removal, check whether the strategy can be removed
    if (!force && !canRemoveStrategy(symbol)) {
        std::stringstream ss;
        ss << "Cannot remove strategy for " << symbol 
           << " - has active position, will keep monitoring";
        LOG_WARN(ss.str());
        
        // Mark as inactive but keep the instance to continue monitoring positions
        it->second.is_active = false;
        return;
    }
    
    // Unsubscribe using the associated exchange instance
    const auto& exchange = it->second.exchange;
    if (exchange && exchange->isConnected()) {
        exchange->unsubscribeKLine(symbol);
        exchange->unsubscribeTick(symbol);
        LOG_INFO("Unsubscribed market data for " + symbol + " from " + it->second.exchange_name);
    }
    
    // Stop the strategy
    if (it->second.strategy) {
        it->second.strategy->stop();
    }
    
    // Erase the instance
    strategy_instances_.erase(it);
    
    std::stringstream ss;
    ss << "Removed strategy instance for " << symbol;
    LOG_INFO(ss.str());
}

bool StrategyManager::hasStrategyInstance(const std::string& symbol) const {
    // No lock needed; caller already holds the lock
    return strategy_instances_.find(symbol) != strategy_instances_.end();
}

void StrategyManager::startAllStrategies() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : strategy_instances_) {
        if (pair.second.strategy && !pair.second.strategy->isRunning()) {
            pair.second.strategy->start();
        }
    }
    
    std::stringstream ss;
    ss << "Started all strategy instances: " << strategy_instances_.size();
    LOG_INFO(ss.str());
}

void StrategyManager::stopAllStrategies() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : strategy_instances_) {
        if (pair.second.strategy && pair.second.strategy->isRunning()) {
            pair.second.strategy->stop();
        }
    }
    
    std::stringstream ss;
    ss << "Stopped all strategy instances: " << strategy_instances_.size();
    LOG_INFO(ss.str());
}

size_t StrategyManager::getActiveStrategyCount() const {
    // No lock needed; caller already holds the lock
    
    return std::count_if(strategy_instances_.begin(), strategy_instances_.end(),
        [](const std::pair<std::string, StrategyInstance>& pair) {
            return pair.second.strategy && pair.second.strategy->isRunning();
        });
}

std::vector<std::string> StrategyManager::getStrategyStockCodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> codes;
    for (const auto& pair : strategy_instances_) {
        codes.push_back(pair.first);
    }
    return codes;
}

void StrategyManager::printStrategyStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (strategy_instances_.empty()) {
        LOG_INFO("No active strategy instances");
        return;
    }
    
    std::stringstream ss;
    ss << "\n=== Strategy Instances (" << strategy_instances_.size() << ") ===";
    
    for (const auto& pair : strategy_instances_) {
        const auto& instance = pair.second;
        ss << "\n  " << instance.symbol 
           << " - " << (instance.strategy->isRunning() ? "RUNNING" : "STOPPED")
           << " - " << (instance.is_active ? "ACTIVE" : "INACTIVE");
    }
    
    LOG_INFO(ss.str());
}

bool StrategyManager::canRemoveStrategy(const std::string& symbol) const {
    // No lock needed; caller already holds the lock

    // Check for existing positions
    auto& pos_mgr = PositionManager::getInstance();
    if (pos_mgr.hasPosition(symbol)) {
        return false;  // has positions; cannot remove
    }
    
    return true;  // no positions; safe to remove
}

std::shared_ptr<StrategyBase> StrategyManager::createStrategy(
    const std::string& symbol, 
    const ScanResult& scan_result) {
    
    // You can select different strategy types based on configuration or conditions
    // Currently defaulting to creating a MomentumStrategy
    
    auto strategy = std::make_shared<MomentumStrategy>();
    
    // Set strategy name (including symbol)
    std::stringstream ss;
    ss << "Momentum_" << symbol << "_" << scan_result.stock_name;
    // Note: Consider adding setName method to StrategyBase or pass name in constructor
    
    return strategy;
}

    // ========== Event handling ==========

void StrategyManager::initializeEventHandlers(IEventEngine* event_engine) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (event_engine == nullptr) {
        LOG_ERROR("Event engine is null");
        return;
    }
    
    event_engine_ = event_engine;
    
    // Register event handlers
    // Capture `this` in lambdas to dispatch to member handlers when events arrive
    kline_handler_id_ = event_engine_->registerHandler(
        EventType::EVENT_KLINE,
        [this](const EventPtr& event) { this->onKLineEvent(event); }
    );
    
    tick_handler_id_ = event_engine_->registerHandler(
        EventType::EVENT_TICK,
        [this](const EventPtr& event) { this->onTickEvent(event); }
    );
    
    trade_handler_id_ = event_engine_->registerHandler(
        EventType::EVENT_TRADE_DEAL,
        [this](const EventPtr& event) { this->onTradeEvent(event); }
    );
    
    LOG_INFO("StrategyManager event handlers registered");
}

void StrategyManager::onKLineEvent(const EventPtr& event) {
    if (!event) {
        return;
    }
    
    // Extract KlineData from the event
    const KlineData* kline = event->getData<KlineData>();
    if (kline == nullptr) {
        LOG_ERROR("Failed to extract KlineData from event");
        return;
    }
    
    std::string symbol = kline->symbol;
    
    // Find the corresponding strategy instance and invoke its handler
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = strategy_instances_.find(symbol);
        if (it == strategy_instances_.end()) {
            // No corresponding strategy instance; return early
            return;
        }
        
        if (!it->second.is_active || !it->second.strategy) {
            return;
        }
        
        // Call the strategy's onKLine method to handle the data
        it->second.strategy->onKLine(symbol, *kline);
    }
}

void StrategyManager::onTickEvent(const EventPtr& event) {
    if (!event) {
        return;
    }
    
    // Extract TickData from the event
    const TickData* tick = event->getData<TickData>();
    if (tick == nullptr) {
        LOG_ERROR("Failed to extract TickData from event");
        return;
    }
    
    std::string symbol = tick->symbol;
    
    // Find the corresponding strategy instance and invoke its handler
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = strategy_instances_.find(symbol);
        if (it == strategy_instances_.end()) {
            // No corresponding strategy instance; return early
            return;
        }
        
        if (!it->second.is_active || !it->second.strategy) {
            return;
        }
        
        // Call the strategy's onTick method to handle the data
        it->second.strategy->onTick(symbol, *tick);
    }
}

void StrategyManager::onTradeEvent(const EventPtr& event) {
    if (!event) {
        return;
    }
    
    // Extract TradeData from the event
    const TradeData* trade = event->getData<TradeData>();
    if (trade == nullptr) {
        LOG_ERROR("Failed to extract TradeData from event");
        return;
    }
    
    std::string symbol = trade->symbol;
    
    // Find the corresponding strategy instance
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = strategy_instances_.find(symbol);
        if (it == strategy_instances_.end()) {
            // No corresponding strategy instance; return early
            return;
        }
        
        if (!it->second.is_active || !it->second.strategy) {
            return;
        }
        
        // You can add trade processing here
        // Optional: record trade events, update positions, compute P&L, etc.
        std::stringstream ss;
        ss << "Trade executed for " << symbol 
           << " - Direction: " << (trade->direction == Direction::LONG ? "LONG" : "SHORT")
           << ", Volume: " << trade->volume 
           << ", Price: " << trade->price;
        LOG_INFO(ss.str());
        
        // TODO: Consider adding an onTrade() method to StrategyBase to handle trade events
    }
}

