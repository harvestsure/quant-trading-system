#pragma once

/**
 * @file notification_example.h
 * @brief Notification system usage example
 *
 * This file demonstrates how to use the notification queue in real applications
 */

/*

=== 1. Basic integration in main.cpp ===

#include "notification/notification_manager.h"
#include "config/config_manager.h"

int main() {
    // Load configuration
    ConfigManager& config_manager = ConfigManager::getInstance();
    config_manager.loadFromJson("config.json");
    
    // Initialize notification system
    NotificationManager& notif_manager = NotificationManager::getInstance();
    if (!notif_manager.initialize()) {
        LOG_ERROR("Failed to initialize notification manager");
        return 1;
    }
    
    // ... other initialization code ...
    
    // Example: send messages
    notif_manager.sendInfo("Trading system started");
    notif_manager.sendTradeSignal("BUY signal detected for stock XYZ at price 50.5");
    notif_manager.sendTradeExecution("Order executed: BUY 100 shares of XYZ");
    
    // ... trading logic ...
    
    // Graceful shutdown
    notif_manager.waitUntilEmpty(10);  // wait for all messages to be sent
    notif_manager.shutdown();
    
    return 0;
}


=== 2. Configuration in config.json ===

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


=== 3. Steps to obtain Telegram Bot Token and Chat ID ===

A. Create a Bot (obtain bot_token):
  1. Search for @BotFather in Telegram
  2. Send the /newbot command
  3. Follow prompts to provide a bot name and username
  4. BotFather returns the Bot Token, e.g.: 123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11

B. Obtain Chat ID:
  Method 1 (private chat):
  1. Send any message to your bot in Telegram
  2. Visit in a browser: https://api.telegram.org/bot<bot_token>/getUpdates
  3. In the returned JSON, find "chat":{"id": 123456789} — that's your chat_id

  Method 2 (group):
  1. Create a private group
  2. Add your bot to the group
  3. Send a message in the group
  4. Use the method above to get the group's chat_id (will be negative)


=== 4. Usage in strategies ===

#include "notification/notification_manager.h"

void MomentumStrategy::onSignal(const SignalEvent& event) {
    auto& notif = NotificationManager::getInstance();
    
    std::string message = "Momentum Signal: " + 
                         event.symbol + " - " + 
                         (event.direction == SignalDirection::BUY ? "BUY" : "SELL") +
                         " at " + std::to_string(event.price);
    
    notif.sendTradeSignal(message);
}


=== 5. Usage in order execution module ===

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


=== 6. Error handling ===

#include "notification/notification_manager.h"

void RiskManager::onRiskViolation(const std::string& violation_detail) {
    auto& notif = NotificationManager::getInstance();
    notif.sendError("⚠️ RISK ALERT: " + violation_detail);
}


=== 7. Inspecting queue status ===

auto& queue = notif_manager.getQueue();
size_t pending_messages = queue.getQueueSize();
size_t sent_count = queue.getSentCount();
size_t failed_count = queue.getFailedCount();

LOG_INFO("Queue status: pending=" << pending_messages 
         << ", sent=" << sent_count 
         << ", failed=" << failed_count);

*/
