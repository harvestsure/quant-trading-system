#pragma once

#include "notification_queue.h"
#include "config/config_manager.h"
#include <memory>
#include <string>

/**
 * @brief 通知管理器，负责初始化和管理所有通知发送器
 */
class NotificationManager {
public:
    /**
     * @brief 获取单例实例
     */
    static NotificationManager& getInstance();
    
    /**
     * @brief 从配置初始化通知系统
     * @param config 交易配置
     * @return true if initialization successful
     */
    bool initialize(const TradingConfig& config);
    
    /**
     * @brief 初始化（从默认配置文件读取）
     */
    bool initialize();
    
    /**
     * @brief 关闭通知系统
     */
    void shutdown();
    
    /**
     * @brief 获取通知队列实例
     */
    NotificationQueue& getQueue() { return NotificationQueue::getInstance(); }
    
    /**
     * @brief 发送交易信号消息
     */
    bool sendTradeSignal(const std::string& message) {
        return NotificationQueue::getInstance().sendMessage(message, "trade_signal");
    }
    
    /**
     * @brief 发送交易执行消息
     */
    bool sendTradeExecution(const std::string& message) {
        return NotificationQueue::getInstance().sendMessage(message, "trade");
    }
    
    /**
     * @brief 发送错误消息
     */
    bool sendError(const std::string& message) {
        return NotificationQueue::getInstance().sendMessage(message, "error");
    }
    
    /**
     * @brief 发送信息消息
     */
    bool sendInfo(const std::string& message) {
        return NotificationQueue::getInstance().sendMessage(message, "info");
    }
    
    /**
     * @brief 等待所有消息发送完成（用于优雅关闭）
     */
    bool waitUntilEmpty(int timeout_seconds = 10) {
        return NotificationQueue::getInstance().waitUntilEmpty(timeout_seconds);
    }
    
    // 禁止拷贝和移动
    NotificationManager(const NotificationManager&) = delete;
    NotificationManager& operator=(const NotificationManager&) = delete;
    NotificationManager(NotificationManager&&) = delete;
    NotificationManager& operator=(NotificationManager&&) = delete;

private:
    NotificationManager() = default;
    ~NotificationManager() = default;
    
    bool initialized_ = false;
};
