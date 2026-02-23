#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include "common/defines.h"
#include "common/object.h"

// Forward declaration
class IEventEngine;

#define ExchangeClass "GetExchangeClass"
#define ExchangeInstance "GetExchangeInstance"

namespace dylib {
    class library;
}
 

// Account information
struct AccountInfo {
    std::string account_id;
    double total_assets;       // total assets
    double cash;               // cash
    double market_value;       // market value
    double available_funds;    // available funds
    double frozen_funds;       // frozen funds
    std::string currency;      // currency
};

// Position information (retrieved from exchange)
struct ExchangePosition {
    std::string symbol;
    std::string stock_name;
    int quantity;
    double avg_price;
    double current_price;
    double market_value;
    double cost_price;
    double profit_loss;
    double profit_loss_ratio;
};

// Exchange interface base class
class IExchange {
public:
    virtual ~IExchange() = default;
    
    // ========== Connection management ==========
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual std::string getName() const = 0;
    virtual std::string getDisplayName() const = 0;
    
    // ========== Account related ==========
    virtual AccountInfo getAccountInfo() = 0;
    virtual std::vector<ExchangePosition> getPositions() = 0;
    virtual double getAvailableFunds() = 0;
    
    // ========== Trading related ==========
    virtual std::string placeOrder(
        const std::string& symbol,
        const std::string& side,      // "BUY" or "SELL"
        int quantity,
        const std::string& order_type, // "MARKET" or "LIMIT"
        double price = 0.0
    ) = 0;
    
    virtual bool cancelOrder(const std::string& order_id) = 0;
    virtual bool modifyOrder(const std::string& order_id, int new_quantity, double new_price) = 0;
    virtual OrderData getOrderStatus(const std::string& order_id) = 0;
    virtual std::vector<OrderData> getOrderHistory(int days = 1) = 0;
    
    // ========== Market data related ==========
    virtual bool subscribeKLine(const std::string& symbol, const std::string& kline_type) = 0;
    virtual bool unsubscribeKLine(const std::string& symbol) = 0;
    virtual bool subscribeTick(const std::string& symbol) = 0;
    virtual bool unsubscribeTick(const std::string& symbol) = 0;
    
    virtual std::vector<KlineData> getHistoryKLine(
        const std::string& symbol,
        const std::string& kline_type,
        int count
    ) = 0;
    
    virtual Snapshot getSnapshot(const std::string& symbol) = 0;
    
    // ========== Market scanning related ==========
    virtual std::vector<std::string> getMarketStockList() = 0;
    virtual std::map<std::string, Snapshot> getBatchSnapshots(const std::vector<std::string>& stock_codes) = 0;
    
    // ========== Event-driven interface ==========
    // Exchange implementations should convert raw data to unified formats and publish events.
    // Callbacks are not used; use the event engine instead.
    // After subscribing to market data, the exchange will convert and publish data to the event engine.
    
    // ========== Event engine ==========
    virtual IEventEngine* getEventEngine() const = 0;
};

// Exchange factory
class ExchangeFactory {
public:
    static ExchangeFactory& getInstance();

    std::shared_ptr<IExchange> createExchange(
        IEventEngine* event_engine,
        std::string name,
        const std::map<std::string, std::string>& config
    );

    ExchangeFactory(const ExchangeFactory&) = delete;
    ExchangeFactory& operator=(const ExchangeFactory&) = delete;

private:
    ExchangeFactory();

    void load_exchange_class();
    void load_exchange_class_from_module(std::string module_name);


    std::map<std::string, std::shared_ptr<dylib::library>> loaded_libraries_;
};

 
