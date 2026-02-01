#pragma once

#include "exchange_interface.h"
#include "common/object.h"
#include <memory>
#include <mutex>

 

// 交易所管理器 - 系统访问交易所的统一入口
class ExchangeManager {
public:
    static ExchangeManager& getInstance();
    
    // 初始化交易所
    bool initExchange(ExchangeType type, const std::map<std::string, std::string>& config);
    
    // 获取交易所实例
    std::shared_ptr<IExchange> getExchange();
    
    // 快捷方法 - 转发到底层交易所
    bool connect();
    bool disconnect();
    bool isConnected() const;
    
    AccountInfo getAccountInfo();
    std::vector<ExchangePosition> getPositions();
    double getAvailableFunds();
    
    std::string placeOrder(
        const std::string& symbol,
        const std::string& side,
        int quantity,
        const std::string& order_type,
        double price = 0.0
    );
    
    bool cancelOrder(const std::string& order_id);
    OrderData getOrderStatus(const std::string& order_id);
    
    bool subscribeKLine(const std::string& symbol, const std::string& kline_type);
    bool unsubscribeKLine(const std::string& symbol);
    bool subscribeTick(const std::string& symbol);
    bool unsubscribeTick(const std::string& symbol);
    
    std::vector<KlineData> getHistoryKLine(
        const std::string& symbol,
        const std::string& kline_type,
        int count
    );
    
    Snapshot getSnapshot(const std::string& symbol);
    std::vector<std::string> getMarketStockList(const std::string& market);
    std::map<std::string, Snapshot> getBatchSnapshots(const std::vector<std::string>& stock_codes);
    
    // Callbacks have been replaced with event engine
    
    // 禁止拷贝
    ExchangeManager(const ExchangeManager&) = delete;
    ExchangeManager& operator=(const ExchangeManager&) = delete;
    
private:
    ExchangeManager() = default;
    
    std::shared_ptr<IExchange> exchange_;
    mutable std::mutex mutex_;
};

 
