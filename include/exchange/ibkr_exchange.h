#pragma once

#include "exchange_interface.h"
#include <string>
#include <mutex>

 

// IBKR (Interactive Brokers) configuration
struct IBKRConfig {
    std::string host = "127.0.0.1";
    int port = 7497;  // 7497 for TWS Paper, 7496 for TWS Live
    int client_id = 0;
    bool is_simulation = true;
};

/**
 * @brief IBKR exchange implementation
 *
 * Wraps the Interactive Brokers TWS API
 * Requires TWS (Trader Workstation) or IB Gateway
 */
class IBKRExchange : public IExchange {
public:
    explicit IBKRExchange(const IBKRConfig& config);
    virtual ~IBKRExchange();
    
    // ========== Connection management ==========
    bool connect() override;
    bool disconnect() override;
    bool isConnected() const override;
    std::string getName() const override { return "ibkr"; }
    std::string getDisplayName() const override { return "Interactive Brokers"; }
    
    // ========== Account related ==========
    AccountInfo getAccountInfo() override;
    std::vector<ExchangePosition> getPositions() override;
    double getAvailableFunds() override;
    
    // ========== Trading related ==========
    std::string placeOrder(
        const std::string& symbol,
        const std::string& side,
        int quantity,
        const std::string& order_type,
        double price = 0.0
    ) override;
    
    bool cancelOrder(const std::string& order_id) override;
    bool modifyOrder(const std::string& order_id, int new_quantity, double new_price) override;
    OrderData getOrderStatus(const std::string& order_id) override;
    std::vector<OrderData> getOrderHistory(int days = 1) override;
    
    // ========== Market data related ==========
    bool subscribeKLine(const std::string& symbol, const std::string& kline_type) override;
    bool unsubscribeKLine(const std::string& symbol) override;
    bool subscribeTick(const std::string& symbol) override;
    bool unsubscribeTick(const std::string& symbol) override;
    
    std::vector<KlineData> getHistoryKLine(
        const std::string& symbol,
        const std::string& kline_type,
        int count
    ) override;
    
    Snapshot getSnapshot(const std::string& symbol) override;
    
    // ========== Market scanning related ==========
    std::vector<std::string> getMarketStockList() override;
    std::map<std::string, Snapshot> getBatchSnapshots(const std::vector<std::string>& stock_codes) override;
    
    // ========== Event engine ==========
    void setEventEngine(IEventEngine* event_engine) override;
    IEventEngine* getEventEngine() const override { return event_engine_; }
    
private:
    IBKRConfig config_;
    bool connected_;
    mutable std::mutex mutex_;
    IEventEngine* event_engine_ = nullptr;  // pointer to event engine
    
    // Helper: publish logs via event engine
    void writeLog(LogLevel level, const std::string& message);
    
    // TWS API related members
    // TODO: add TWS API client object
    // EClientSocket* client_socket_;
    // EWrapper* wrapper_;
    
    // Data conversion methods (convert IBKR raw data to unified format)
    OrderData convertIBKROrder(const void* ibkr_order);
    Snapshot convertIBKRSnapshot(const void* ibkr_snapshot);
    ExchangePosition convertIBKRPosition(const void* ibkr_position);
    
    // Event publishing methods (internal use)
    void publishTickEvent(const std::string& symbol, const void* ibkr_tick);
    void publishKLineEvent(const std::string& symbol, const void* ibkr_bar);
    void publishOrderEvent(const OrderData& order);
    void publishTradeEvent(const void* ibkr_execution);
};

 
