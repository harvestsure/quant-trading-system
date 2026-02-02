#include "exchange/exchange_manager.h"
#include "config/config_manager.h"
#include "common/object.h"
#include "utils/logger.h"

ExchangeManager& ExchangeManager::getInstance() {
    static ExchangeManager instance;
    return instance;
}

bool ExchangeManager::initExchange(const ExchangeInstanceConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ExchangeType type;
    std::map<std::string, std::string> params;
    
    if (config.name == "futu") {
        type = ExchangeType::FUTU;
    } else if (config.name == "ibkr") {
        type = ExchangeType::IBKR;
    } else if (config.name == "binance") {
        type = ExchangeType::BINANCE;
    } else {
        LOG_ERROR("Unknown exchange type: " + config.name);
        return false;
    }
    
    // 将JSON参数转换为字符串map
    for (const auto& [key, value] : config.params.items()) {
        if (value.is_string()) {
            params[key] = value.get<std::string>();
        } else if (value.is_number()) {
            params[key] = value.dump();
        } else if (value.is_boolean()) {
            params[key] = value.get<bool>() ? "true" : "false";
        } else {
            params[key] = value.dump();
        }
    }
    
    auto exchange = ExchangeFactory::createExchange(type, params);
    if (!exchange) {
        LOG_ERROR("Failed to create exchange: " + config.name);
        return false;
    }
    
    exchanges_[config.name] = exchange;
        
    LOG_INFO("Exchange initialized: " + config.name);
    return true;
}

bool ExchangeManager::initAllExchanges(const std::vector<ExchangeInstanceConfig>& configs) {
    for (const auto& config : configs) {
        if (config.is_enabled) {
            if (!initExchange(config)) {
                LOG_WARNING("Failed to initialize exchange: " + config.name);
            }
        }
    }
    
    if (exchanges_.empty()) {
        LOG_ERROR("No exchanges initialized");
        return false;
    }
    
    return true;
}

std::shared_ptr<IExchange> ExchangeManager::getExchange(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (name.empty()) {
		LOG_ERROR("Exchange name is empty");
		return nullptr;
    }
    
    auto it = exchanges_.find(name);
    if (it != exchanges_.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<IExchange>> ExchangeManager::getAllExchanges() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::shared_ptr<IExchange>> result;
    for (const auto& pair : exchanges_) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<std::shared_ptr<IExchange>> ExchangeManager::getEnabledExchanges() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::shared_ptr<IExchange>> result;
    for (const auto& pair : exchanges_) {
        if (pair.second && pair.second->isConnected()) {
            result.push_back(pair.second);
        }
    }
    return result;
}

bool ExchangeManager::connect(const std::string& exchange_name) {
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return false;
    }
    return exchange->connect();
}

bool ExchangeManager::disconnect(const std::string& exchange_name) {
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        return true;
    }
    return exchange->disconnect();
}

bool ExchangeManager::isConnected(const std::string& exchange_name) {
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        return true;
    }
    return exchange->isConnected();
}

AccountInfo ExchangeManager::getAccountInfo(const std::string& exchange_name) {
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return AccountInfo();
    }
    return exchange->getAccountInfo();
}

std::vector<ExchangePosition> ExchangeManager::getPositions(const std::string& exchange_name) {
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return {};
    }
    return exchange->getPositions();
}

double ExchangeManager::getAvailableFunds(const std::string& exchange_name) {
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return 0.0;
    }
    return exchange->getAvailableFunds();
}

std::string ExchangeManager::placeOrder(
    const std::string& exchange_name,
    const std::string& symbol,
    const std::string& side,
    int quantity,
    const std::string& order_type,
    double price) {
    
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return "";
    }
    return exchange->placeOrder(symbol, side, quantity, order_type, price);
}

bool ExchangeManager::cancelOrder(const std::string& exchange_name, const std::string& order_id) {
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return false;
    }
    return exchange->cancelOrder(order_id);
}

OrderData ExchangeManager::getOrderStatus(const std::string& exchange_name, const std::string& order_id) {
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return OrderData();
    }
    return exchange->getOrderStatus(order_id);
}

bool ExchangeManager::subscribeKLine(const std::string& exchange_name, const std::string& symbol, const std::string& kline_type) {
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return false;
    }
    return exchange->subscribeKLine(symbol, kline_type);
}

bool ExchangeManager::unsubscribeKLine(const std::string& exchange_name, const std::string& symbol) {
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        return false;
    }
    return exchange->unsubscribeKLine(symbol);
}

bool ExchangeManager::subscribeTick(const std::string& exchange_name, const std::string& symbol) {
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return false;
    }
    return exchange->subscribeTick(symbol);
}

bool ExchangeManager::unsubscribeTick(const std::string& exchange_name, const std::string& symbol) {
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        return false;
    }
    return exchange->unsubscribeTick(symbol);
}

std::vector<KlineData> ExchangeManager::getHistoryKLine(
    const std::string& exchange_name,
    const std::string& symbol,
    const std::string& kline_type,
    int count) {
    
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return {};
    }
    return exchange->getHistoryKLine(symbol, kline_type, count);
}
Snapshot ExchangeManager::getSnapshot(const std::string& exchange_name, const std::string& symbol) {
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return Snapshot();
    }
    return exchange->getSnapshot(symbol);
}

std::vector<std::string> ExchangeManager::getMarketStockList(const std::string& exchange_name) {
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return {};
    }
    return exchange->getMarketStockList();
}

std::map<std::string, Snapshot> ExchangeManager::getBatchSnapshots(
    const std::string& exchange_name,
    const std::vector<std::string>& stock_codes) {
    
    auto exchange = getExchange(exchange_name);
    if (!exchange) {
        LOG_ERROR("Exchange not initialized");
        return {};
    }
    return exchange->getBatchSnapshots(stock_codes);
}

// Callbacks have been replaced with event engine
// No longer using callback registration in ExchangeManager

