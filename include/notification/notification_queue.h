#pragma once

#include <string>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

/**
 * @brief 消息对象，用于在队列中传输
 */
struct NotificationMessage {
    std::string id;           // 消息唯一ID
    std::string content;      // 消息内容
    std::string type;         // 消息类型（trade, signal, error, info等）
    int64_t timestamp;        // 消息时间戳
    int retry_count = 0;      // 重试次数
    
    NotificationMessage() = default;
    NotificationMessage(const std::string& content, const std::string& type = "info")
        : content(content), type(type) {}
};

/**
 * @brief 通知消息发送器的抽象接口
 */
class INotificationSender {
public:
    virtual ~INotificationSender() = default;
    
    /**
     * @brief 发送消息
     * @param message 待发送的消息
     * @return true if success, false otherwise
     */
    virtual bool send(const NotificationMessage& message) = 0;
    
    /**
     * @brief 检查发送器是否就绪
     */
    virtual bool isReady() const = 0;
};

/**
 * @brief 消息通知队列，负责消息的缓存和分发
 */
class NotificationQueue {
public:
    /**
     * @brief 获取单例实例
     */
    static NotificationQueue& getInstance();
    
    /**
     * @brief 初始化队列，启动后台处理线程
     * @param max_queue_size 队列最大容量
     * @return true if success
     */
    bool initialize(size_t max_queue_size = 1000);
    
    /**
     * @brief 关闭队列，停止处理线程
     */
    void shutdown();
    
    /**
     * @brief 将消息加入队列
     * @param message 待加入的消息
     * @return true if added successfully, false if queue is full or not initialized
     */
    bool enqueue(const NotificationMessage& message);
    
    /**
     * @brief 便捷方法：直接发送文本消息
     * @param content 消息内容
     * @param type 消息类型
     * @return true if enqueued successfully
     */
    bool sendMessage(const std::string& content, const std::string& type = "info");
    
    /**
     * @brief 注册消息发送器
     * @param sender 消息发送器实现
     */
    void registerSender(std::shared_ptr<INotificationSender> sender);
    
    /**
     * @brief 获取当前队列大小
     */
    size_t getQueueSize() const;
    
    /**
     * @brief 获取已发送消息数
     */
    size_t getSentCount() const { return sent_count_; }
    
    /**
     * @brief 获取失败消息数
     */
    size_t getFailedCount() const { return failed_count_; }
    
    /**
     * @brief 等待队列清空（用于优雅关闭）
     * @param timeout_seconds 超时时间（秒）
     * @return true if queue is empty before timeout
     */
    bool waitUntilEmpty(int timeout_seconds = 10);
    
    // 禁止拷贝和移动
    NotificationQueue(const NotificationQueue&) = delete;
    NotificationQueue& operator=(const NotificationQueue&) = delete;
    NotificationQueue(NotificationQueue&&) = delete;
    NotificationQueue& operator=(NotificationQueue&&) = delete;

private:
    NotificationQueue() = default;
    ~NotificationQueue();
    
    /**
     * @brief 后台处理线程的主函数
     */
    void processingThread();
    
    /**
     * @brief 处理单条消息
     */
    void processMessage(const NotificationMessage& message);
    
    // 队列和线程管理
    std::queue<NotificationMessage> message_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    std::atomic<bool> running_ = false;
    std::atomic<bool> initialized_ = false;
    std::unique_ptr<std::thread> worker_thread_;
    
    size_t max_queue_size_ = 1000;
    std::atomic<size_t> sent_count_ = 0;
    std::atomic<size_t> failed_count_ = 0;
    
    // 消息发送器
    std::vector<std::shared_ptr<INotificationSender>> senders_;
    mutable std::mutex sender_mutex_;
};
