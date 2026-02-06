#pragma once

/**
 * @file notification_example.h
 * @brief 通知系统使用示例
 * 
 * 本文件展示了如何在实际应用中使用消息通知队列
 */

/*

=== 1. 在 main.cpp 中的基本集成 ===

#include "notification/notification_manager.h"
#include "config/config_manager.h"

int main() {
    // 加载配置
    ConfigManager& config_manager = ConfigManager::getInstance();
    config_manager.loadFromJson("config.json");
    
    // 初始化通知系统
    NotificationManager& notif_manager = NotificationManager::getInstance();
    if (!notif_manager.initialize()) {
        LOG_ERROR("Failed to initialize notification manager");
        return 1;
    }
    
    // ... 其他初始化代码 ...
    
    // 示例：发送消息
    notif_manager.sendInfo("Trading system started");
    notif_manager.sendTradeSignal("BUY signal detected for stock XYZ at price 50.5");
    notif_manager.sendTradeExecution("Order executed: BUY 100 shares of XYZ");
    
    // ... 交易逻辑 ...
    
    // 优雅关闭
    notif_manager.waitUntilEmpty(10);  // 等待所有消息发送完成
    notif_manager.shutdown();
    
    return 0;
}


=== 2. 在 config.json 中的配置 ===

{
  "notification": {
    "telegram": {
      "enabled": true,
      "bot_token": "123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11",
      "chat_id": "987654321",
      "api_timeout_seconds": 5,
      "max_queue_size": 1000,
      "batch_send": false,
      "batch_size": 10,
      "batch_interval_ms": 1000
    }
  }
}


=== 3. 获取 Telegram Bot Token 和 Chat ID 的步骤 ===

A. 创建 Bot（获取 bot_token）：
   1. 在 Telegram 中搜索 @BotFather
   2. 发送 /newbot 命令
   3. 按照提示输入 Bot 名称和用户名
   4. BotFather 会返回 Bot Token，格式如: 123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11

B. 获取 Chat ID：
   方法1（个人聊天）：
   1. 直接与你创建的 Bot 聊天，发送任意消息
   2. 在浏览器访问: https://api.telegram.org/bot<bot_token>/getUpdates
   3. 在返回的 JSON 中，找到 "chat":{"id": 123456789}，那就是你的 chat_id
   
   方法2（创建群组）：
   1. 创建一个私有群组
   2. 添加你的 Bot 到群组
   3. 群组发送一条消息
   4. 使用上面的方法获取 group 的 chat_id（会是负数）


=== 4. 在策略中使用 ===

#include "notification/notification_manager.h"

void MomentumStrategy::onSignal(const SignalEvent& event) {
    auto& notif = NotificationManager::getInstance();
    
    std::string message = "Momentum Signal: " + 
                         event.symbol + " - " + 
                         (event.direction == SignalDirection::BUY ? "BUY" : "SELL") +
                         " at " + std::to_string(event.price);
    
    notif.sendTradeSignal(message);
}


=== 5. 在订单执行模块中使用 ===

#include "notification/notification_manager.h"

void OrderExecutor::onOrderFilled(const Order& order) {
    auto& notif = NotificationManager::getInstance();
    
    std::string message = "Order Filled: " + 
                         order.symbol + " " +
                         (order.side == OrderSide::BUY ? "BUY" : "SELL") + " " +
                         std::to_string(order.quantity) + " @ " +
                         std::to_string(order.filled_price);
    
    notif.sendTradeExecution(message);
}


=== 6. 错误处理 ===

#include "notification/notification_manager.h"

void RiskManager::onRiskViolation(const std::string& violation_detail) {
    auto& notif = NotificationManager::getInstance();
    notif.sendError("⚠️ RISK ALERT: " + violation_detail);
}


=== 7. 检查队列状态 ===

auto& queue = notif_manager.getQueue();
size_t pending_messages = queue.getQueueSize();
size_t sent_count = queue.getSentCount();
size_t failed_count = queue.getFailedCount();

LOG_INFO("Queue status: pending=" << pending_messages 
         << ", sent=" << sent_count 
         << ", failed=" << failed_count);

*/
