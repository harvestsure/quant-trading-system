#pragma once

#include "notification_queue.h"
#include <nlohmann/json.hpp>
#include <string>
#include <memory>

/**
 * @brief Telegram Bot message sender
 * Uses cpp-httplib to send HTTP POST requests to the Telegram Bot API
 */
class TelegramSender : public INotificationSender {
public:
    /**
     * @brief Constructor
     * @param bot_token Telegram Bot token
     * @param chat_id target chat ID
     * @param timeout_seconds HTTP request timeout in seconds
     */
    TelegramSender(const std::string& bot_token, const std::string& chat_id, int timeout_seconds = 5);
    
    /**
     * @brief Destructor
     */
    ~TelegramSender() override;
    
    /**
     * @brief Send a notification
     * @param message the message to send
     * @return true if sent successfully
     */
    bool send(const NotificationMessage& message) override;
    
    /**
     * @brief Check if sender is ready (validates bot_token and chat_id)
     * @return true if bot token and chat id are valid
     */
    bool isReady() const override;
    
    /**
     * @brief Test whether connection to Telegram API is reachable
     * @return true if can reach Telegram API
     */
    bool testConnection();
    
    /**
     * @brief Create a TelegramSender from configuration JSON
     * @param config JSON object containing bot_token, chat_id, api_timeout_seconds
     * @return shared_ptr<TelegramSender> or nullptr if validation fails
     */
    static std::shared_ptr<TelegramSender> createFromConfig(const nlohmann::json& config);

private:
    /**
     * @brief Format message for Telegram
     */
    std::string formatMessage(const NotificationMessage& message) const;
    
    /**
     * @brief Send HTTP POST request to Telegram API
     */
    bool sendHttpRequest(const std::string& message_text);
    
    std::string bot_token_;
    std::string chat_id_;
    int timeout_seconds_;
    
    // Telegram Bot API constants
    static constexpr const char* TELEGRAM_API_HOST = "api.telegram.org";
    static constexpr int TELEGRAM_API_PORT = 443;
};
