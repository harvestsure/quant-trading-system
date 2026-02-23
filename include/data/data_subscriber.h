#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <memory>
#include "common/object.h"

 

// Data callback function types (kept for compatibility; use structures from common/object.h)
using KLineCallback = std::function<void(const std::string&, const KlineData&)>;
using TickCallback = std::function<void(const std::string&, const TickData&)>;
using SnapshotCallback = std::function<void(const Snapshot&)>;

class DataSubscriber {
public:
    static DataSubscriber& getInstance();
    
    // Subscribe to K-line (candlestick) data
    bool subscribeKLine(const std::string& symbol, const std::string& kline_type);
    void unsubscribeKLine(const std::string& symbol);
    
    // Subscribe to tick data
    bool subscribeTick(const std::string& symbol);
    void unsubscribeTick(const std::string& symbol);
    
    // Register callback handlers
    void registerKLineCallback(KLineCallback callback);
    void registerTickCallback(TickCallback callback);
    void registerSnapshotCallback(SnapshotCallback callback);
    
    // Retrieve historical K-line data
    std::vector<KlineData> getHistoryKLine(
        const std::string& symbol,
        const std::string& kline_type,
        int count
    );
    
    // Get snapshot data
    Snapshot getSnapshot(const std::string& symbol);
    
    // Non-copyable
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
    
    // Data handlers
    void onKLineData(const std::string& symbol, const KlineData& kline);
    void onTickData(const std::string& symbol, const TickData& tick);
    void onSnapshotData(const Snapshot& snapshot);
};

 
