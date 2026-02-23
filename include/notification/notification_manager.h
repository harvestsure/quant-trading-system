#pragma once

#include "notification_queue.h"
#include "config/config_manager.h"
#include <memory>
#include <string>

/**
 * @brief Notification manager responsible for initializing and managing all notification senders
 */
class NotificationManager {
public:
    /**
     * @brief Get singleton instance
     */
    static NotificationManager& getInstance();
    
    /**
     * @brief Initialize notification system from configuration
     * @param config trading configuration
     * @return true if initialization successful
     */
    bool initialize(const TradingConfig& config);
    
    /**
     * @brief Initialize (reads from default config file)
     */
    bool initialize();
    
    /**
     * @brief Shutdown notification system
     */
    void shutdown();
    
    /**
     * @brief Get notification queue instance
     */
    NotificationQueue& getQueue() { return NotificationQueue::getInstance(); }
    
    /**
     * @brief Send trade signal message
     */
    bool sendTradeSignal(const std::string& message) {
        return NotificationQueue::getInstance().sendMessage(message, "trade_signal");
    }
    
    /**
     * @brief Send trade execution message
     */
    bool sendTradeExecution(const std::string& message) {
        return NotificationQueue::getInstance().sendMessage(message, "trade");
    }
    
    /**
     * @brief Send error message
     */
    bool sendError(const std::string& message) {
        return NotificationQueue::getInstance().sendMessage(message, "error");
    }
    
    /**
     * @brief Send info message
     */
    bool sendInfo(const std::string& message) {
        return NotificationQueue::getInstance().sendMessage(message, "info");
    }
    
    /**
     * @brief Wait for all messages to be sent (for graceful shutdown)
     */
    bool waitUntilEmpty(int timeout_seconds = 10) {
        return NotificationQueue::getInstance().waitUntilEmpty(timeout_seconds);
    }
    
    // Non-copyable and non-movable
    NotificationManager(const NotificationManager&) = delete;
    NotificationManager& operator=(const NotificationManager&) = delete;
    NotificationManager(NotificationManager&&) = delete;
    NotificationManager& operator=(NotificationManager&&) = delete;

private:
    NotificationManager() = default;
    ~NotificationManager() = default;
    
    bool initialized_ = false;
};
