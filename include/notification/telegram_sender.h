#pragma once

#include "notification_queue.h"
#include <nlohmann/json.hpp>
#include <string>
#include <memory>

/**
 * @brief Telegram Bot消息发送器
 * 使用cpp-httplib发送HTTP POST请求到Telegram Bot API
 */
class TelegramSender : public INotificationSender {
public:
    /**
     * @brief 构造函数
     * @param bot_token Telegram Bot Token
     * @param chat_id 目标Chat ID
     * @param timeout_seconds HTTP请求超时时间（秒）
     */
    TelegramSender(const std::string& bot_token, const std::string& chat_id, int timeout_seconds = 5);
    
    /**
     * @brief 析构函数
     */
    ~TelegramSender() override;
    
    /**
     * @brief 发送消息
     * @param message 待发送的消息
     * @return true if sent successfully
     */
    bool send(const NotificationMessage& message) override;
    
    /**
     * @brief 检查发送器是否就绪（验证bot_token和chat_id）
     * @return true if bot token and chat id are valid
     */
    bool isReady() const override;
    
    /**
     * @brief 测试连接是否正常
     * @return true if can reach Telegram API
     */
    bool testConnection();
    
    /**
     * @brief 从配置JSON创建Telegram发送器
     * @param config 包含 bot_token, chat_id, api_timeout_seconds 的JSON对象
     * @return shared_ptr<TelegramSender> or nullptr if validation fails
     */
    static std::shared_ptr<TelegramSender> createFromConfig(const nlohmann::json& config);

private:
    /**
     * @brief 构建Telegram消息格式
     */
    std::string formatMessage(const NotificationMessage& message) const;
    
    /**
     * @brief 发送HTTP POST请求到Telegram API
     */
    bool sendHttpRequest(const std::string& message_text);
    
    std::string bot_token_;
    std::string chat_id_;
    int timeout_seconds_;
    
    // Telegram Bot API 常量
    static constexpr const char* TELEGRAM_API_HOST = "api.telegram.org";
    static constexpr int TELEGRAM_API_PORT = 443;
};
