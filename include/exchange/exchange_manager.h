#pragma once

#include "exchange_interface.h"
#include "config/config_manager.h"
#include "common/object.h"
#include <memory>
#include <mutex>

 

// 交易所管理器 - 支持多交易所的统一入口
class ExchangeManager {
public:
    static ExchangeManager& getInstance();
    
    // 初始化单个交易所
    bool initExchange(const ExchangeInstanceConfig& config);
    
    // 初始化所有启用的交易所
    bool initAllExchanges(const std::vector<ExchangeInstanceConfig>& configs);
    
    // 获取交易所实例
    std::shared_ptr<IExchange> getExchange(const std::string& exchange_name);
    std::vector<std::shared_ptr<IExchange>> getAllExchanges();
    std::vector<std::shared_ptr<IExchange>> getEnabledExchanges();

    // 快捷方法 - 转发到主交易所
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
    std::vector<std::string> getMarketStockList(const std::string& exchange_name, const std::string& market);
    std::map<std::string, Snapshot> getBatchSnapshots(const std::string& exchange_name, const std::vector<std::string>& stock_codes);
    
    // Callbacks have been replaced with event engine
    
    // 禁止拷贝
    ExchangeManager(const ExchangeManager&) = delete;
    ExchangeManager& operator=(const ExchangeManager&) = delete;
    
private:
    ExchangeManager() = default;
    
    std::map<std::string, std::shared_ptr<IExchange>> exchanges_;  // 多交易所存储
    mutable std::mutex mutex_;
};

 
