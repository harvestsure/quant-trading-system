#include "notification/notification_manager.h"
#include "config/config_manager.h"
#include "utils/logger.h"
#include <iostream>
#include <thread>
#include <chrono>

/**
 * @brief Demo of how to use the notification queue system
 * This program demonstrates the full initialization flow and various message sending methods
 */

int main() {
    try {
        LOG_INFO("=== Notification System Demo ===");
        
        // ============================================
        // Step 1: Load configuration
        // ============================================
        LOG_INFO("Step 1: Loading configuration...");
        ConfigManager& config_mgr = ConfigManager::getInstance();
        
        // Attempt to load config.json; if missing, defaults will be used
        if (!config_mgr.loadFromJson("config.json")) {
            LOG_WARN("config.json not found, using default settings");
            // The system will use default settings
        }
        
        // ============================================
        // Step 2: Initialize notification system
        // ============================================
        LOG_INFO("Step 2: Initializing notification system...");
        NotificationManager& notif_mgr = NotificationManager::getInstance();
        
        if (!notif_mgr.initialize()) {
            LOG_ERROR("Failed to initialize notification manager");
            LOG_INFO("Please check config.json for notification settings");
            return 1;
        }
        
        LOG_INFO("Notification system initialized successfully!");
        
        // ============================================
        // Step 3: Get queue status
        // ============================================
        auto& queue = notif_mgr.getQueue();
        LOG_INFO("Queue size: " << queue.getQueueSize());
        LOG_INFO("Sent count: " << queue.getSentCount());
        LOG_INFO("Failed count: " << queue.getFailedCount());
        
        // ============================================
        // Step 4: Send different types of test messages
        // ============================================
        LOG_INFO("Step 3: Sending test messages...");
        
        // Send an info message
        notif_mgr.sendInfo("🟢 Trading system started successfully");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Send a trade signal message
        notif_mgr.sendTradeSignal("📊 MOMENTUM SIGNAL:\n"
                                  "Symbol: AAPL\n"
                                  "Side: BUY\n"
                                  "Price: 150.25\n"
                                  "Confidence: 85%");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Send a trade execution message
        notif_mgr.sendTradeExecution("✅ ORDER EXECUTED:\n"
                                     "Symbol: AAPL\n"
                                     "Side: BUY\n"
                                     "Quantity: 100 shares\n"
                                     "Price: 150.26\n"
                                     "Total: $15,026.00");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Send an error message
        notif_mgr.sendError("⚠️ RISK ALERT:\n"
                           "Daily loss exceeded threshold\n"
                           "Current loss: 2.5%\n"
                           "Max allowed: 2.0%\n"
                           "Action: Positions locked");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Use the generic sendMessage method (custom type)
        NotificationQueue::getInstance().sendMessage(
            "Custom message with custom type",
            "custom_event"
        );
        
        // ============================================
        // Step 5: Wait for messages to be sent
        // ============================================
        LOG_INFO("Step 4: Waiting for all messages to be sent...");
        LOG_INFO("Queue size: " << queue.getQueueSize());
        
        // Wait for the queue to empty (timeout: 10 seconds)
        if (notif_mgr.waitUntilEmpty(10)) {
            LOG_INFO("All messages sent successfully!");
        } else {
            LOG_WARN("Timeout waiting for queue to empty, some messages may still be pending");
        }
        
        // ============================================
        // Step 6: Final statistics
        // ============================================
        LOG_INFO("=== Final Statistics ===");
        LOG_INFO("Sent count: " << queue.getSentCount());
        LOG_INFO("Failed count: " << queue.getFailedCount());
        LOG_INFO("Queue size: " << queue.getQueueSize());
        
        // ============================================
        // Step 7: Graceful shutdown
        // ============================================
        LOG_INFO("Step 5: Shutting down notification system...");
        notif_mgr.shutdown();
        
        LOG_INFO("=== Demo completed successfully! ===");
        
        return 0;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception occurred: " << e.what());
        return 1;
    }
}


/*
=== How to run this demo ===

1. Build the project
    cd /Users/sure/Code/quant-trading-system
    mkdir build && cd build
    cmake ..
    make

2. Configure Telegram (optional)

    To actually send Telegram messages, you need to:
    a. Create config.json (copy from config.json.example)
    b. Generate a Telegram Bot Token (see NOTIFICATION_SYSTEM.md)
    c. Obtain the Chat ID (see NOTIFICATION_SYSTEM.md)
    d. Update the notification settings in config.json

    {
      "notification": {
         "telegram": {
            "enabled": true,
            "bot_token": "YOUR_BOT_TOKEN",
            "chat_id": "YOUR_CHAT_ID"
         }
      }
    }

3. Run the demo
    ./notification_demo

    Check the logs to confirm messages were enqueued and sent

4. Verify Telegram messages
    You should see 5 demo messages in the chat you configured

=== Example output ===

[INFO] === Notification System Demo ===
[INFO] Step 1: Loading configuration...
[INFO] Step 2: Initializing notification system...
[INFO] TelegramSender created successfully with timeout: 5s
[INFO] Telegram sender registered successfully, total senders: 1
[INFO] NotificationManager initialized successfully
[INFO] Step 3: Sending test messages...
[INFO] All messages sent successfully!
[INFO] === Final Statistics ===
[INFO] Sent count: 5
[INFO] Failed count: 0
[INFO] Queue size: 0
[INFO] Step 5: Shutting down notification system...
[INFO] === Demo completed successfully! ===

*/
