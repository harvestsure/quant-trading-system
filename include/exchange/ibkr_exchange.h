#pragma once

#include "exchange_interface.h"
#include <string>
#include <mutex>

 

// IBKR (Interactive Brokers) 配置
struct IBKRConfig {
    std::string host = "127.0.0.1";
    int port = 7497;  // 7497 for TWS Paper, 7496 for TWS Live
    int client_id = 0;
    bool is_simulation = true;
};

/**
 * @brief IBKR交易所实现类
 * 
 * 封装Interactive Brokers TWS API
 * 需要安装TWS (Trader Workstation) 或 IB Gateway
 */
class IBKRExchange : public IExchange {
public:
    explicit IBKRExchange(const IBKRConfig& config);
    virtual ~IBKRExchange();
    
    // ========== 连接管理 ==========
    bool connect() override;
    bool disconnect() override;
    bool isConnected() const override;
    std::string getName() const override { return "ibkr"; }
    std::string getDisplayName() const override { return "Interactive Brokers"; }
    
    // ========== 账户相关 ==========
    AccountInfo getAccountInfo() override;
    std::vector<ExchangePosition> getPositions() override;
    double getAvailableFunds() override;
    
    // ========== 交易相关 ==========
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
    
    // ========== 行情数据相关 ==========
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
    
    // ========== 市场扫描相关 ==========
    std::vector<std::string> getMarketStockList() override;
    std::map<std::string, Snapshot> getBatchSnapshots(const std::vector<std::string>& stock_codes) override;
    
    // ========== 事件引擎 ==========
    void setEventEngine(IEventEngine* event_engine) override;
    IEventEngine* getEventEngine() const override { return event_engine_; }
    
private:
    IBKRConfig config_;
    bool connected_;
    mutable std::mutex mutex_;
    IEventEngine* event_engine_ = nullptr;  // 事件引擎指针
    
    // 辅助方法：通过事件引擎发布日志
    void writeLog(LogLevel level, const std::string& message);
    
    // TWS API相关成员变量
    // TODO: 添加TWS API的客户端对象
    // EClientSocket* client_socket_;
    // EWrapper* wrapper_;
    
    // 数据转换方法（将IBKR原始数据转换为统一格式）
    OrderData convertIBKROrder(const void* ibkr_order);
    Snapshot convertIBKRSnapshot(const void* ibkr_snapshot);
    ExchangePosition convertIBKRPosition(const void* ibkr_position);
    
    // 事件发布方法（内部使用）
    void publishTickEvent(const std::string& symbol, const void* ibkr_tick);
    void publishKLineEvent(const std::string& symbol, const void* ibkr_bar);
    void publishOrderEvent(const OrderData& order);
    void publishTradeEvent(const void* ibkr_execution);
};

 
