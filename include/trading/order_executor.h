#pragma once

#include <string>
#include <map>
#include <mutex>
#include "common/object.h"
 

class OrderExecutor {
public:
    static OrderExecutor& getInstance();
    
    // 下单
    std::string placeOrder(
        const std::string& symbol,
        OrderSide side,
        int quantity,
        OrderType type = OrderType::MARKET,
        double price = 0.0
    );
    
    // 撤单
    bool cancelOrder(const std::string& order_id);
    
    // 查询订单
    OrderData* getOrder(const std::string& order_id);
    std::map<std::string, OrderData> getAllOrders();
    
    // 订单状态更新回调（由Futu API调用）
    void onOrderUpdate(const OrderData& order);
    
    // 禁止拷贝
    OrderExecutor(const OrderExecutor&) = delete;
    OrderExecutor& operator=(const OrderExecutor&) = delete;
    
private:
    OrderExecutor() = default;
    
    std::map<std::string, OrderData> orders_;
    mutable std::mutex mutex_;
    
    std::string generateOrderId();
};

 
