#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include "common/defines.h"
#include "common/object.h"

// 前向声明
class IEventEngine;

#define ExchangeClass "GetExchangeClass"
#define ExchangeInstance "GetExchangeInstance"

namespace dylib {
    class library;
}
 

// 账户信息
struct AccountInfo {
    std::string account_id;
    double total_assets;       // 总资产
    double cash;               // 现金
    double market_value;       // 市值
    double available_funds;    // 可用资金
    double frozen_funds;       // 冻结资金
    std::string currency;      // 货币
};

// 持仓信息（从交易所获取）
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

// 交易所接口基类
class IExchange {
public:
    virtual ~IExchange() = default;
    
    // ========== 连接管理 ==========
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual std::string getName() const = 0;
    virtual std::string getDisplayName() const = 0;
    
    // ========== 账户相关 ==========
    virtual AccountInfo getAccountInfo() = 0;
    virtual std::vector<ExchangePosition> getPositions() = 0;
    virtual double getAvailableFunds() = 0;
    
    // ========== 交易相关 ==========
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
    
    // ========== 行情数据相关 ==========
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
    
    // ========== 市场扫描相关 ==========
    virtual std::vector<std::string> getMarketStockList() = 0;
    virtual std::map<std::string, Snapshot> getBatchSnapshots(const std::vector<std::string>& stock_codes) = 0;
    
    // ========== 事件驱动接口 ==========
    // 交易所实现类应该将原始数据转换为统一格式并发布事件
    // 不再使用回调，改用事件引擎
    // 订阅行情后，交易所会自动将数据转换并发布到事件引擎
    
    // ========== 事件引擎 ==========
    virtual void setEventEngine(IEventEngine* event_engine) = 0;
    virtual IEventEngine* getEventEngine() const = 0;
};

// 交易所工厂
class ExchangeFactory {
public:
    static ExchangeFactory& getInstance();

    std::shared_ptr<IExchange> createExchange(
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

 
