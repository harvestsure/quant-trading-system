#pragma once

#include "exchange_interface.h"
#include <mutex>
#include <vector>
#include <map>
#include <thread>

// 前向声明
class IEventEngine;

// Forward declarations
#ifdef ENABLE_FUTU
namespace Futu {
    class FTAPI_Qot;
    class FTAPI_Trd;
}
namespace Qot_Common {
    class Security;
}
class FutuSpi;
#endif

// Futu API 配置
struct FutuConfig {
    std::string host = "127.0.0.1";
    int port = 11111;
    std::string unlock_password = "";
    bool is_simulation = true;
    std::string market = "HK";
};

// Futu交易所实现
class FutuExchange : public IExchange {
    friend class FutuSpi;
public:
    explicit FutuExchange(const FutuConfig& config);
    virtual ~FutuExchange();
    
    // ========== 连接管理 ==========
    bool connect() override;
    bool disconnect() override;
    bool isConnected() const override;
    std::string getName() const override { return "futu"; }
    std::string getDisplayName() const override { return "Futu Securities"; }
    
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

protected:
    // 辅助方法：通过事件引擎发布日志
    void writeLog(LogLevel level, const std::string& message);

private:
    FutuConfig config_;
    bool connected_;
    mutable std::mutex mutex_;
    IEventEngine* event_engine_ = nullptr;  // 事件引擎指针
    
    #ifdef ENABLE_FUTU
    FutuSpi* spi_ = nullptr;              // 回调处理和API管理
    std::vector<uint64_t> account_ids_;   // 账户列表
    #endif
    
    // Futu API 相关内部方法
    bool unlockTrade();
    bool getAccountList();
    
    #ifdef ENABLE_FUTU
    Qot_Common::Security convertToSecurity(const std::string& symbol);
    int32_t convertKLineType(const std::string& kline_type);
    #endif
};


extern "C"
{
	QTS_DECL_EXPORT const char* GetExchangeClass();
	QTS_DECL_EXPORT IExchange* GetExchangeInstance(const std::map<std::string, std::string>& config);
}