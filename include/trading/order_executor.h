#pragma once

#include <string>
#include <map>
#include <mutex>
#include "common/object.h"
 

class OrderExecutor {
public:
    static OrderExecutor& getInstance();
    
    // Place an order
    std::string placeOrder(
        const std::string& symbol,
        OrderSide side,
        int quantity,
        OrderType type = OrderType::MARKET,
        double price = 0.0
    );
    
    // Cancel an order
    bool cancelOrder(const std::string& order_id);
    
    // Query order
    OrderData* getOrder(const std::string& order_id);
    std::map<std::string, OrderData> getAllOrders();
    
    // Order status update callback (invoked by Futu API)
    void onOrderUpdate(const OrderData& order);
    
    // Non-copyable
    OrderExecutor(const OrderExecutor&) = delete;
    OrderExecutor& operator=(const OrderExecutor&) = delete;
    
private:
    OrderExecutor() = default;
    
    std::map<std::string, OrderData> orders_;
    mutable std::mutex mutex_;
    
    std::string generateOrderId();
};

 
