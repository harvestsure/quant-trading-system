#include "notification/notification_manager.h"
#include "config/config_manager.h"
#include "utils/logger.h"
#include <iostream>
#include <thread>
#include <chrono>

/**
 * @brief 演示如何使用消息通知队列系统
 * 本程序展示了完整的初始化流程和各种消息发送方式
 */

int main() {
    try {
        LOG_INFO("=== Notification System Demo ===");
        
        // ============================================
        // 步骤1：加载配置
        // ============================================
        LOG_INFO("Step 1: Loading configuration...");
        ConfigManager& config_mgr = ConfigManager::getInstance();
        
        // 尝试加载 config.json，如果不存在会失败，这是正常的
        if (!config_mgr.loadFromJson("config.json")) {
            LOG_WARN("config.json not found, using default settings");
            // 系统会使用默认配置
        }
        
        // ============================================
        // 步骤2：初始化通知系统
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
        // 步骤3：获取队列状态
        // ============================================
        auto& queue = notif_mgr.getQueue();
        LOG_INFO("Queue size: " << queue.getQueueSize());
        LOG_INFO("Sent count: " << queue.getSentCount());
        LOG_INFO("Failed count: " << queue.getFailedCount());
        
        // ============================================
        // 步骤4：发送不同类型的消息
        // ============================================
        LOG_INFO("Step 3: Sending test messages...");
        
        // 发送信息消息
        notif_mgr.sendInfo("🟢 Trading system started successfully");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 发送交易信号
        notif_mgr.sendTradeSignal("📊 MOMENTUM SIGNAL:\n"
                                  "Symbol: AAPL\n"
                                  "Side: BUY\n"
                                  "Price: 150.25\n"
                                  "Confidence: 85%");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 发送交易执行消息
        notif_mgr.sendTradeExecution("✅ ORDER EXECUTED:\n"
                                     "Symbol: AAPL\n"
                                     "Side: BUY\n"
                                     "Quantity: 100 shares\n"
                                     "Price: 150.26\n"
                                     "Total: $15,026.00");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 发送错误消息
        notif_mgr.sendError("⚠️ RISK ALERT:\n"
                           "Daily loss exceeded threshold\n"
                           "Current loss: 2.5%\n"
                           "Max allowed: 2.0%\n"
                           "Action: Positions locked");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 使用通用消息发送方法（自定义类型）
        NotificationQueue::getInstance().sendMessage(
            "Custom message with custom type",
            "custom_event"
        );
        
        // ============================================
        // 步骤5：等待消息发送完成
        // ============================================
        LOG_INFO("Step 4: Waiting for all messages to be sent...");
        LOG_INFO("Queue size: " << queue.getQueueSize());
        
        // 等待队列清空（最多等待10秒）
        if (notif_mgr.waitUntilEmpty(10)) {
            LOG_INFO("All messages sent successfully!");
        } else {
            LOG_WARN("Timeout waiting for queue to empty, some messages may still be pending");
        }
        
        // ============================================
        // 步骤6：查看最终统计
        // ============================================
        LOG_INFO("=== Final Statistics ===");
        LOG_INFO("Sent count: " << queue.getSentCount());
        LOG_INFO("Failed count: " << queue.getFailedCount());
        LOG_INFO("Queue size: " << queue.getQueueSize());
        
        // ============================================
        // 步骤7：优雅关闭
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
=== 如何运行此演示 ===

1. 编译项目
   cd /Users/sure/Code/quant-trading-system
   mkdir build && cd build
   cmake ..
   make

2. 配置 Telegram（可选）
   
   如果要实际发送Telegram消息，需要：
   a. 创建 config.json（从 config.json.example 复制）
   b. 生成 Telegram Bot Token （见 NOTIFICATION_SYSTEM.md）
   c. 获取 Chat ID （见 NOTIFICATION_SYSTEM.md）
   d. 更新 config.json 中的通知配置
   
   {
     "notification": {
       "telegram": {
         "enabled": true,
         "bot_token": "YOUR_BOT_TOKEN",
         "chat_id": "YOUR_CHAT_ID"
       }
     }
   }

3. 运行演示程序
   ./notification_demo
   
   查看日志输出来确认消息是否成功入队和发送

4. 检查 Telegram 接收到的消息
   在你配置的Chat中应该会看到5条演示消息

=== 输出示例 ===

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
