#include "exchange/exchange_manager.h"
#include "common/object.h"
#include "utils/logger.h"

ExchangeManager& ExchangeManager::getInstance() {
    static ExchangeManager instance;
    return instance;
}

bool ExchangeManager::initExchange(
    ExchangeType type,
    const std::map<std::string, std::string>& config) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (exchange_) {
        LOG_WARNING("Exchange already initialized");
        return true;
    }
    
    exchange_ = ExchangeFactory::createExchange(type, config);
    
    if (!exchange_) {
        LOG_ERROR("Failed to create exchange");
        return false;
    }
    
    LOG_INFO("Exchange initialized successfully");
    return true;
}

std::shared_ptr<IExchange> ExchangeManager::getExchange() {
    std::lock_guard<std::mutex> lock(mutex_);
    return exchange_;
}

bool ExchangeManager::connect() {
    auto exchange = getExchange();
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return false;
    }
    return exchange->connect();
}

bool ExchangeManager::disconnect() {
    auto exchange = getExchange();
    if (!exchange) {
        return true;
    }
    return exchange->disconnect();
}

bool ExchangeManager::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!exchange_) {
        return false;
    }
    return exchange_->isConnected();
}

AccountInfo ExchangeManager::getAccountInfo() {
    auto exchange = getExchange();
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return AccountInfo();
    }
    return exchange->getAccountInfo();
}

std::vector<ExchangePosition> ExchangeManager::getPositions() {
    auto exchange = getExchange();
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return {};
    }
    return exchange->getPositions();
}

double ExchangeManager::getAvailableFunds() {
    auto exchange = getExchange();
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return 0.0;
    }
    return exchange->getAvailableFunds();
}

std::string ExchangeManager::placeOrder(
    const std::string& symbol,
    const std::string& side,
    int quantity,
    const std::string& order_type,
    double price) {
    
    auto exchange = getExchange();
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return "";
    }
    return exchange->placeOrder(symbol, side, quantity, order_type, price);
}

bool ExchangeManager::cancelOrder(const std::string& order_id) {
    auto exchange = getExchange();
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return false;
    }
    return exchange->cancelOrder(order_id);
}

OrderData ExchangeManager::getOrderStatus(const std::string& order_id) {
    auto exchange = getExchange();
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return OrderData();
    }
    return exchange->getOrderStatus(order_id);
}

bool ExchangeManager::subscribeKLine(const std::string& symbol, const std::string& kline_type) {
    auto exchange = getExchange();
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return false;
    }
    return exchange->subscribeKLine(symbol, kline_type);
}

bool ExchangeManager::unsubscribeKLine(const std::string& symbol) {
    auto exchange = getExchange();
    if (!exchange) {
        return false;
    }
    return exchange->unsubscribeKLine(symbol);
}

bool ExchangeManager::subscribeTick(const std::string& symbol) {
    auto exchange = getExchange();
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return false;
    }
    return exchange->subscribeTick(symbol);
}

bool ExchangeManager::unsubscribeTick(const std::string& symbol) {
    auto exchange = getExchange();
    if (!exchange) {
        return false;
    }
    return exchange->unsubscribeTick(symbol);
}

std::vector<KlineData> ExchangeManager::getHistoryKLine(
    const std::string& symbol,
    const std::string& kline_type,
    int count) {
    
    auto exchange = getExchange();
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return {};
    }
    return exchange->getHistoryKLine(symbol, kline_type, count);
}
Snapshot ExchangeManager::getSnapshot(const std::string& symbol) {
    auto exchange = getExchange();
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return Snapshot();
    }
    return exchange->getSnapshot(symbol);
}

std::vector<std::string> ExchangeManager::getMarketStockList(const std::string& market) {
    auto exchange = getExchange();
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return {};
    }
    return exchange->getMarketStockList(market);
}

std::map<std::string, Snapshot> ExchangeManager::getBatchSnapshots(
    const std::vector<std::string>& stock_codes) {
    
    auto exchange = getExchange();
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return {};
    }
    return exchange->getBatchSnapshots(stock_codes);
}

// Callbacks have been replaced with event engine
// No longer using callback registration in ExchangeManager

