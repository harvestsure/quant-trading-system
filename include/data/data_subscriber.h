#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <memory>
#include "common/object.h"

 

// 数据回调函数类型（保留为兼容性，使用 common/object.h 中的数据结构）
using KLineCallback = std::function<void(const std::string&, const KlineData&)>;
using TickCallback = std::function<void(const std::string&, const TickData&)>;
using SnapshotCallback = std::function<void(const Snapshot&)>;

class DataSubscriber {
public:
    static DataSubscriber& getInstance();
    
    // 订阅K线数据
    bool subscribeKLine(const std::string& symbol, const std::string& kline_type);
    void unsubscribeKLine(const std::string& symbol);
    
    // 订阅Tick数据
    bool subscribeTick(const std::string& symbol);
    void unsubscribeTick(const std::string& symbol);
    
    // 注册回调函数
    void registerKLineCallback(KLineCallback callback);
    void registerTickCallback(TickCallback callback);
    void registerSnapshotCallback(SnapshotCallback callback);
    
    // 获取历史K线数据
    std::vector<KlineData> getHistoryKLine(
        const std::string& symbol,
        const std::string& kline_type,
        int count
    );
    
    // 获取快照数据
    Snapshot getSnapshot(const std::string& symbol);
    
    // 禁止拷贝
    DataSubscriber(const DataSubscriber&) = delete;
    DataSubscriber& operator=(const DataSubscriber&) = delete;
    
private:
    DataSubscriber() = default;
    
    std::map<std::string, std::string> kline_subscriptions_;  // symbol -> kline_type
    std::map<std::string, bool> tick_subscriptions_;
    
    std::vector<KLineCallback> kline_callbacks_;
    std::vector<TickCallback> tick_callbacks_;
    std::vector<SnapshotCallback> snapshot_callbacks_;
    
    mutable std::mutex mutex_;
    
    // 数据处理
    void onKLineData(const std::string& symbol, const KlineData& kline);
    void onTickData(const std::string& symbol, const TickData& tick);
    void onSnapshotData(const Snapshot& snapshot);
};

 
