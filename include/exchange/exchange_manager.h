#pragma once

#include "exchange_interface.h"
#include "config/config_manager.h"
#include "common/object.h"
#include <memory>
#include <mutex>

// Forward declaration
class IEventEngine;

 

// Exchange manager - unified entry supporting multiple exchanges
class ExchangeManager {
public:
    static ExchangeManager& getInstance();
    
    // Initialize a single exchange
    bool initExchange(const ExchangeInstanceConfig& config);
    
    // Initialize all enabled exchanges
    bool initAllExchanges(const std::vector<ExchangeInstanceConfig>& configs);
    
    // Set the event engine (should be called before initializing exchanges)
    void setEventEngine(IEventEngine* event_engine);
    
    // Get exchange instance
    std::shared_ptr<IExchange> getExchange(const std::string& exchange_name);
    std::vector<std::shared_ptr<IExchange>> getAllExchanges();
    std::vector<std::shared_ptr<IExchange>> getEnabledExchanges();

    // Convenience methods - forward to the main exchange
    bool connect(const std::string& exchange_name);
    bool disconnect(const std::string& exchange_name);
    bool isConnected(const std::string& exchange_name);
    
    AccountInfo getAccountInfo(const std::string& exchange_name);
    std::vector<ExchangePosition> getPositions(const std::string& exchange_name);
    double getAvailableFunds(const std::string& exchange_name);
    
    std::string placeOrder(
        const std::string& exchange_name,
        const std::string& symbol,
        const std::string& side,
        int quantity,
        const std::string& order_type,
        double price = 0.0
    );
    
    bool cancelOrder(const std::string& exchange_name, const std::string& order_id);
    OrderData getOrderStatus(const std::string& exchange_name, const std::string& order_id);
    
    bool subscribeKLine(const std::string& exchange_name, const std::string& symbol, const std::string& kline_type);
    bool unsubscribeKLine(const std::string& exchange_name, const std::string& symbol);
    bool subscribeTick(const std::string& exchange_name, const std::string& symbol);
    bool unsubscribeTick(const std::string& exchange_name, const std::string& symbol);
    
    std::vector<KlineData> getHistoryKLine(
        const std::string& exchange_name,
        const std::string& symbol,
        const std::string& kline_type,
        int count
    );
    
    Snapshot getSnapshot(const std::string& exchange_name, const std::string& symbol);
    std::vector<std::string> getMarketStockList(const std::string& exchange_name);
    std::map<std::string, Snapshot> getBatchSnapshots(const std::string& exchange_name, const std::vector<std::string>& stock_codes);
    
    // Callbacks have been replaced with the event engine

    // Non-copyable
    ExchangeManager(const ExchangeManager&) = delete;
    ExchangeManager& operator=(const ExchangeManager&) = delete;
    
private:
    ExchangeManager() = default;
    
    std::map<std::string, std::shared_ptr<IExchange>> exchanges_;  // storage for multiple exchanges
    mutable std::mutex mutex_;
    IEventEngine* event_engine_ = nullptr;  // pointer to event engine
};

 
