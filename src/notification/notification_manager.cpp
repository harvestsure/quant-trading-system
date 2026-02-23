#include "notification/notification_manager.h"
#include "notification/telegram_sender.h"
#include "utils/logger.h"

NotificationManager& NotificationManager::getInstance() {
    static NotificationManager instance;
    return instance;
}

bool NotificationManager::initialize(const TradingConfig& config) {
    if (initialized_) {
        LOG_INFO("NotificationManager is already initialized");
        return true;
    }
    
    // Initialize the notification queue
    auto& queue = NotificationQueue::getInstance();
    queue.initialize(config.notification.telegram.max_queue_size);
    
    // Register Telegram sender
    if (config.notification.telegram.enabled) {
        auto telegram_sender = TelegramSender::createFromConfig(
            nlohmann::json::object({
                {"bot_token", config.notification.telegram.bot_token},
                {"chat_id", config.notification.telegram.chat_id},
                {"api_timeout_seconds", config.notification.telegram.api_timeout_seconds}
            })
        );
        
        if (telegram_sender) {
            queue.registerSender(telegram_sender);
            LOG_INFO("Telegram sender registered successfully");
            
            // Test connection
            if (telegram_sender->testConnection()) {
                LOG_INFO("Telegram bot connection test passed");
            } else {
                LOG_WARN("Telegram bot connection test failed");
            }
        } else {
            LOG_ERROR("Failed to create Telegram sender");
            return false;
        }
    } else {
        LOG_INFO("Telegram notification is disabled");
    }
    
    initialized_ = true;
    LOG_INFO("NotificationManager initialized successfully");
    return true;
}

bool NotificationManager::initialize() {
    const auto& config = ConfigManager::getInstance().getConfig();
    return initialize(config);
}

void NotificationManager::shutdown() {
    if (!initialized_) {
        return;
    }
    
    LOG_INFO("Shutting down NotificationManager...");
    NotificationQueue::getInstance().shutdown();
    initialized_ = false;
}
