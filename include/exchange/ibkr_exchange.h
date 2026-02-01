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
    
    // ========== 市场数据订阅 ==========
    bool subscribeKLine(const std::string& symbol, const std::string& period) override;
    bool unsubscribeKLine(const std::string& symbol) override;
    bool subscribeTick(const std::string& symbol) override;
    bool unsubscribeTick(const std::string& symbol) override;
    
    // ========== 历史数据获取 ==========
    std::vector<KLine> getHistoryKLine(
        const std::string& symbol,
        const std::string& period,
        int count) override;
    
    // ========== 行情快照 ==========
    Snapshot getSnapshot(const std::string& symbol) override;
    std::vector<Snapshot> getMarketSnapshot(const std::vector<std::string>& symbols) override;
    
    // ========== 交易接口 ==========
    std::string placeOrder(const OrderRequest& request) override;
    bool cancelOrder(const std::string& order_id) override;
    bool modifyOrder(const std::string& order_id, double price, int quantity) override;
    
    // ========== 订单查询 ==========
    Order getOrder(const std::string& order_id) override;
    std::vector<Order> getTodayOrders() override;
    std::vector<Order> getHistoryOrders(const std::string& start_date, const std::string& end_date) override;
    
    // ========== 持仓查询 ==========
    std::vector<ExchangePosition> getPositions() override;
    ExchangePosition getPosition(const std::string& symbol) override;
    
    // ========== 账户查询 ==========
    AccountInfo getAccountInfo() override;
    
private:
    IBKRConfig config_;
    bool connected_;
    mutable std::mutex mutex_;
    
    // TWS API相关成员变量
    // TODO: 添加TWS API的客户端对象
    // EClientSocket* client_socket_;
    // EWrapper* wrapper_;
    
    // 数据转换方法（将IBKR原始数据转换为统一格式）
    Order convertIBKROrder(const void* ibkr_order);
    Snapshot convertIBKRSnapshot(const void* ibkr_snapshot);
    ExchangePosition convertIBKRPosition(const void* ibkr_position);
    
    // 事件发布方法（内部使用）
    void publishTickEvent(const std::string& symbol, const void* ibkr_tick);
    void publishKLineEvent(const std::string& symbol, const void* ibkr_bar);
    void publishOrderEvent(const Order& order);
    void publishTradeEvent(const void* ibkr_execution);
};

 
