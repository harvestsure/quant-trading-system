#include "strategies/strategy_base.h"
#include "data/data_subscriber.h"
#include "trading/order_executor.h"
#include "managers/position_manager.h"
#include "managers/risk_manager.h"
#include "utils/logger.h"
#include <sstream>

StrategyBase::StrategyBase(const std::string& name) 
    : name_(name), running_(false) {
    
    std::stringstream ss;
    ss << "Strategy created: " << name_;
    LOG_INFO(ss.str());
}

void StrategyBase::start() {
    if (running_) {
        LOG_WARN("Strategy already running: " + name_);
        return;
    }
    
    running_ = true;
    
    std::stringstream ss;
    ss << "Strategy started: " << name_;
    LOG_INFO(ss.str());
}

void StrategyBase::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Unsubscribe from all subscriptions
    auto& subscriber = DataSubscriber::getInstance();
    for (const auto& pair : subscribed_stocks_) {
        subscriber.unsubscribeKLine(pair.first);
        subscriber.unsubscribeTick(pair.first);
    }
    subscribed_stocks_.clear();
    
    std::stringstream ss;
    ss << "Strategy stopped: " << name_;
    LOG_INFO(ss.str());
}

void StrategyBase::onKLine(const std::string& symbol, const KlineData& kline) {
    // Default implementation is empty; subclasses may override
    (void)symbol;
    (void)kline;

    LOG_INFO("Strategy " + name_ + " received KLine data for " + symbol);
}

void StrategyBase::onTick(const std::string& symbol, const TickData& tick) {
    // Default implementation is empty; subclasses may override
    (void)symbol;
    (void)tick;

    LOG_INFO("Strategy " + name_ + " received Tick data for " + symbol);
}

void StrategyBase::onSnapshot(const Snapshot& snapshot) {
    // Default implementation is empty; subclasses may override
    (void)snapshot;

    LOG_INFO("Strategy " + name_ + " received Snapshot data");
}

void StrategyBase::subscribeStock(const std::string& symbol) {
    if (subscribed_stocks_.find(symbol) != subscribed_stocks_.end()) {
        return;  // already subscribed
    }
    
    auto& subscriber = DataSubscriber::getInstance();
    
    // Subscribe to 5-minute KLine and Tick data
    subscriber.subscribeKLine(symbol, "K_5M");
    subscriber.subscribeTick(symbol);
    
    subscribed_stocks_[symbol] = true;
    
    std::stringstream ss;
    ss << "Strategy " << name_ << " subscribed: " << symbol;
    LOG_INFO(ss.str());
}

void StrategyBase::unsubscribeStock(const std::string& symbol) {
    auto it = subscribed_stocks_.find(symbol);
    if (it == subscribed_stocks_.end()) {
        return;  // not subscribed
    }
    
    auto& subscriber = DataSubscriber::getInstance();
    subscriber.unsubscribeKLine(symbol);
    subscriber.unsubscribeTick(symbol);
    
    subscribed_stocks_.erase(it);
    
    std::stringstream ss;
    ss << "Strategy " << name_ << " unsubscribed: " << symbol;
    LOG_INFO(ss.str());
}

bool StrategyBase::buy(const std::string& symbol, int quantity, double price) {
    auto& executor = OrderExecutor::getInstance();
    
    // Use market order if price is 0
    OrderType order_type = (price == 0.0) ? OrderType::MARKET : OrderType::LIMIT;
    
    std::string order_id = executor.placeOrder(
        symbol,
        OrderSide::BUY,
        quantity,
        order_type,
        price
    );
    
    if (order_id.empty()) {
        std::stringstream ss;
        ss << "Strategy " << name_ << " failed to buy " << symbol;
        LOG_ERROR(ss.str());
        return false;
    }
    
    std::stringstream ss;
    ss << "Strategy " << name_ << " bought " << quantity << " of " << symbol;
    LOG_INFO(ss.str());
    
    return true;
}

bool StrategyBase::sell(const std::string& symbol, int quantity, double price) {
    auto& executor = OrderExecutor::getInstance();
    
    // Use market order if price is 0
    OrderType order_type = (price == 0.0) ? OrderType::MARKET : OrderType::LIMIT;
    
    std::string order_id = executor.placeOrder(
        symbol,
        OrderSide::SELL,
        quantity,
        order_type,
        price
    );
    
    if (order_id.empty()) {
        std::stringstream ss;
        ss << "Strategy " << name_ << " failed to sell " << symbol;
        LOG_ERROR(ss.str());
        return false;
    }
    
    std::stringstream ss;
    ss << "Strategy " << name_ << " sold " << quantity << " of " << symbol;
    LOG_INFO(ss.str());
    
    return true;
}

std::vector<KlineData> StrategyBase::getHistoryKLine(
    const std::string& symbol,
    const std::string& kline_type,
    int count) {
    
    auto& subscriber = DataSubscriber::getInstance();
    return subscriber.getHistoryKLine(symbol, kline_type, count);
}

