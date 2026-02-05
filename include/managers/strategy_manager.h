#pragma once

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <set>
#include <mutex>
#include "common/object.h"
#include "event/event_interface.h"
#include "exchange/exchange_interface.h"

 

class StrategyBase;

struct ScanResult {
    std::string symbol;
    std::string stock_name;
    double price;
    double change_ratio;
    double volume;
    double turnover_rate;
    double score;  // 评分
    std::string exchange_name;  // 交易所名称
    std::shared_ptr<IExchange> exchange;  // 交易所实例
};

// 策略实例信息
struct StrategyInstance {
    std::string symbol;
    std::shared_ptr<StrategyBase> strategy;
    bool is_active;
    std::string exchange_name;  // 交易所名称
    std::shared_ptr<IExchange> exchange;  // 对应的交易所实例
};

class StrategyManager {
public:
    static StrategyManager& getInstance();
    
    // 初始化事件系统
    void initializeEventHandlers(IEventEngine* event_engine);
    
    // 动态策略管理 - 根据扫描结果创建/删除策略实例
    void processScanResults(const std::vector<ScanResult>& results);
    
    // 策略实例管理
    void createStrategyInstance(const std::string& symbol, const ScanResult& scan_result);
    void removeStrategyInstance(const std::string& symbol, bool force = false);
    bool hasStrategyInstance(const std::string& symbol) const;
    
    // 启动/停止所有策略
    void startAllStrategies();
    void stopAllStrategies();
    
    // 策略状态
    size_t getActiveStrategyCount() const;
    std::vector<std::string> getStrategyStockCodes() const;
    void printStrategyStatus() const;
    
    // 禁止拷贝
    StrategyManager(const StrategyManager&) = delete;
    StrategyManager& operator=(const StrategyManager&) = delete;
    
private:
    StrategyManager() = default;
    
    // 股票代码 -> 策略实例
    std::map<std::string, StrategyInstance> strategy_instances_;
    
    // 上一次扫描的股票代码集合
    std::set<std::string> last_scan_stocks_;
    
    // 事件引擎指针
    IEventEngine* event_engine_ = nullptr;
    
    // 事件处理器IDs
    int kline_handler_id_ = -1;
    int tick_handler_id_ = -1;
    int trade_handler_id_ = -1;
    
    mutable std::mutex mutex_;
    
    // 内部辅助函数
    bool canRemoveStrategy(const std::string& symbol) const;
    std::shared_ptr<StrategyBase> createStrategy(const std::string& symbol, const ScanResult& scan_result);
    
    // 事件处理函数
    void onKLineEvent(const EventPtr& event);
    void onTickEvent(const EventPtr& event);
    void onTradeEvent(const EventPtr& event);
};

 
