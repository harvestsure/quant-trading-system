#include "trading/order_executor.h"
#include "managers/position_manager.h"
#include "managers/risk_manager.h"
#include "utils/logger.h"
#include <sstream>
#include <chrono>
#include <iomanip>

// 注意：这里需要包含Futu API的头文件
// #include "ftdc_trader_api.h"

OrderExecutor& OrderExecutor::getInstance() {
    static OrderExecutor instance;
    return instance;
}

std::string OrderExecutor::placeOrder(
    const std::string& symbol,
    OrderSide side,
    int quantity,
    OrderType type,
    double price) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 风险检查
    auto& risk_mgr = RiskManager::getInstance();
    int signed_qty = (side == OrderSide::BUY) ? quantity : -quantity;
    
    if (!risk_mgr.checkOrderRisk(symbol, signed_qty, price)) {
        LOG_ERROR("Order rejected by risk manager");
        return "";
    }
    
    // 创建订单
    OrderData order;
    order.order_id = generateOrderId();
    order.symbol = symbol;
    order.type = type;
    order.direction = (side == OrderSide::BUY) ? Direction::LONG : Direction::SHORT;
    order.volume = quantity;
    order.price = price;
    order.status = OrderStatus::SUBMITTING;
    order.traded_volume = 0;
    
    auto now = std::chrono::system_clock::now();
    order.create_time = std::chrono::system_clock::to_time_t(now) * 1000;
    order.update_time = order.create_time;
    
    // TODO: 调用Futu API下单
    /*
    FTDC_Trader_API* api = FTDC_Trader_API::getInstance();
    bool success = api->placeOrder(order);
    if (!success) {
        LOG_ERROR("Failed to place order via Futu API");
        return "";
    }
    */
    
    orders_[order.order_id] = order;
    
    std::stringstream log_ss;
    log_ss << "Order placed: " << order.order_id << " " << symbol 
           << " " << (side == OrderSide::BUY ? "BUY" : "SELL")
           << " " << quantity << " @ " << price;
    LOG_INFO(log_ss.str());
    
    // 模拟订单立即成交（实际应该由Futu API回调通知）
    order.status = OrderStatus::FILLED;
    order.traded_volume = quantity;
    onOrderUpdate(order);
    
    return order.order_id;
}

bool OrderExecutor::cancelOrder(const std::string& order_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        LOG_ERROR("Order not found: " + order_id);
        return false;
    }
    
    OrderData& order = it->second;
    
    if (order.status == OrderStatus::FILLED || 
        order.status == OrderStatus::CANCELLED) {
        LOG_WARNING("Cannot cancel order in current status: " + order_id);
        return false;
    }
    
    // TODO: 调用Futu API撤单
    /*
    FTDC_Trader_API* api = FTDC_Trader_API::getInstance();
    bool success = api->cancelOrder(order_id);
    if (!success) {
        LOG_ERROR("Failed to cancel order via Futu API");
        return false;
    }
    */
    
    order.status = OrderStatus::CANCELLED;
    
    std::stringstream ss;
    ss << "Order cancelled: " << order_id;
    LOG_INFO(ss.str());
    
    return true;
}

OrderData* OrderExecutor::getOrder(const std::string& order_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = orders_.find(order_id);
    if (it != orders_.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::map<std::string, OrderData> OrderExecutor::getAllOrders() {
    std::lock_guard<std::mutex> lock(mutex_);
    return orders_;
}

void OrderExecutor::onOrderUpdate(const OrderData& order) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 更新订单状态
    orders_[order.order_id] = order;
    
    // 如果订单成交，更新持仓
    if (order.status == OrderStatus::FILLED || 
        order.status == OrderStatus::PARTIAL_FILLED) {
        
        auto& pos_mgr = PositionManager::getInstance();
        int signed_qty = (order.direction == Direction::LONG) ? 
            order.traded_volume : -order.traded_volume;
        
        pos_mgr.updatePosition(order.symbol, signed_qty, order.price);
        
        std::stringstream ss;
        ss << "Order filled: " << order.order_id << " " << order.traded_volume 
           << " @ " << order.price;
        LOG_INFO(ss.str());
    }
}

std::string OrderExecutor::generateOrderId() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::stringstream ss;
    ss << "ORD" << ms;
    return ss.str();
}

