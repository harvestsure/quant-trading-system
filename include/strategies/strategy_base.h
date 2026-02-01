#pragma once

#include "managers/strategy_manager.h"
#include "data/data_subscriber.h"
#include <string>
#include <atomic>
#include <map>

 

class StrategyBase {
public:
    explicit StrategyBase(const std::string& name);
    virtual ~StrategyBase() = default;
    
    // 策略生命周期
    virtual void start();
    virtual void stop();
    bool isRunning() const { return running_; }
    
    // 策略名称
    std::string getName() const { return name_; }
    
    // 处理扫描结果
    virtual void onScanResult(const ScanResult& result) = 0;
    
    // 数据回调（子类可以选择性覆盖）
    virtual void onKLine(const std::string& symbol, const KlineData& kline);
    virtual void onTick(const std::string& symbol, const TickData& tick);
    virtual void onSnapshot(const Snapshot& snapshot);
    
protected:
    std::string name_;
    std::atomic<bool> running_;
    
    // 订阅的股票列表
    std::map<std::string, bool> subscribed_stocks_;
    
    // 辅助方法
    void subscribeStock(const std::string& symbol);
    void unsubscribeStock(const std::string& symbol);
    
    // 交易方法
    bool buy(const std::string& symbol, int quantity, double price = 0.0);
    bool sell(const std::string& symbol, int quantity, double price = 0.0);
    
    // 获取历史数据
    std::vector<KlineData> getHistoryKLine(
        const std::string& symbol,
        const std::string& kline_type,
        int count
    );
};

 
