#include "data/data_subscriber.h"
#include "utils/logger.h"
#include <sstream>
#include <algorithm>

// Note: include Futu API headers here if integrating with Futu
// #include "ftdc_quote_api.h"

DataSubscriber& DataSubscriber::getInstance() {
    static DataSubscriber instance;
    return instance;
}

bool DataSubscriber::subscribeKLine(const std::string& symbol, const std::string& kline_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // TODO: Call Futu API to subscribe to KLine
    /*
    FTDC_Quote_API* api = FTDC_Quote_API::getInstance();
    bool success = api->subscribeKLine(symbol, kline_type);
    if (!success) {
        LOG_ERROR("Failed to subscribe KLine for " + symbol);
        return false;
    }
    */
    
    kline_subscriptions_[symbol] = kline_type;
    
    std::stringstream ss;
    ss << "Subscribed KLine: " << symbol << " type=" << kline_type;
    LOG_INFO(ss.str());
    
    return true;
}

void DataSubscriber::unsubscribeKLine(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // TODO: Call Futu API to unsubscribe
    /*
    FTDC_Quote_API* api = FTDC_Quote_API::getInstance();
    api->unsubscribeKLine(symbol);
    */
    
    kline_subscriptions_.erase(symbol);
    
    std::stringstream ss;
    ss << "Unsubscribed KLine: " << symbol;
    LOG_INFO(ss.str());
}

bool DataSubscriber::subscribeTick(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // TODO: Call Futu API to subscribe to Tick
    /*
    FTDC_Quote_API* api = FTDC_Quote_API::getInstance();
    bool success = api->subscribeTick(symbol);
    if (!success) {
        LOG_ERROR("Failed to subscribe Tick for " + symbol);
        return false;
    }
    */
    
    tick_subscriptions_[symbol] = true;
    
    std::stringstream ss;
    ss << "Subscribed Tick: " << symbol;
    LOG_INFO(ss.str());
    
    return true;
}

void DataSubscriber::unsubscribeTick(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // TODO: Call Futu API to unsubscribe
    /*
    FTDC_Quote_API* api = FTDC_Quote_API::getInstance();
    api->unsubscribeTick(symbol);
    */
    
    tick_subscriptions_.erase(symbol);
    
    std::stringstream ss;
    ss << "Unsubscribed Tick: " << symbol;
    LOG_INFO(ss.str());
}

void DataSubscriber::registerKLineCallback(KLineCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    kline_callbacks_.push_back(callback);
    LOG_INFO("KLine callback registered");
}

void DataSubscriber::registerTickCallback(TickCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    tick_callbacks_.push_back(callback);
    LOG_INFO("Tick callback registered");
}

void DataSubscriber::registerSnapshotCallback(SnapshotCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_callbacks_.push_back(callback);
    LOG_INFO("Snapshot callback registered");
}

std::vector<KlineData> DataSubscriber::getHistoryKLine(
    const std::string& symbol,
    const std::string& kline_type,
    int count) {
    
    std::vector<KlineData> klines;
    
    // TODO: Call Futu API to get historical KLine
    /*
    FTDC_Quote_API* api = FTDC_Quote_API::getInstance();
    api->getHistoryKLine(symbol, kline_type, count, klines);
    */
    
    std::stringstream ss;
    ss << "Retrieved " << klines.size() << " history KLines for " << symbol << " " << kline_type << " count=" << count;
    LOG_INFO(ss.str());
    
    return klines;
}

Snapshot DataSubscriber::getSnapshot(const std::string& symbol) {
    Snapshot snapshot;
    snapshot.symbol = symbol;
    
    // TODO: Call Futu API to get snapshot
    /*
    FTDC_Quote_API* api = FTDC_Quote_API::getInstance();
    api->getSnapshot(symbol, snapshot);
    */
    
    std::stringstream ss;
    ss << "Retrieved snapshot for " << symbol;
    LOG_INFO(ss.str());
    
    return snapshot;
}

void DataSubscriber::onKLineData(const std::string& symbol, const KlineData& kline) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Trigger all callbacks
    for (auto& callback : kline_callbacks_) {
        callback(symbol, kline);
    }
}

void DataSubscriber::onTickData(const std::string& symbol, const TickData& tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Trigger all callbacks
    for (auto& callback : tick_callbacks_) {
        callback(symbol, tick);
    }
}

void DataSubscriber::onSnapshotData(const Snapshot& snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Trigger all callbacks
    for (auto& callback : snapshot_callbacks_) {
        callback(snapshot);
    }
}

