#pragma once

#include <string>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

/**
 * @brief Notification message object for queue transport
 */
struct NotificationMessage {
    std::string id;           // unique message ID
    std::string content;      // message content
    std::string type;         // message type (trade, signal, error, info, etc.)
    int64_t timestamp;        // message timestamp
    int retry_count = 0;      // retry count
    
    NotificationMessage() = default;
    NotificationMessage(const std::string& content, const std::string& type = "info")
        : content(content), type(type) {}
};

/**
 * @brief Abstract interface for notification senders
 */
class INotificationSender {
public:
    virtual ~INotificationSender() = default;
    
    /**
     * @brief Send a message
     * @param message the message to send
     * @return true if success, false otherwise
     */
    virtual bool send(const NotificationMessage& message) = 0;
    
    /**
     * @brief Check whether the sender is ready
     */
    virtual bool isReady() const = 0;
};

/**
 * @brief Notification queue responsible for buffering and dispatching messages
 */
class NotificationQueue {
public:
    /**
     * @brief Get singleton instance
     */
    static NotificationQueue& getInstance();
    
    /**
     * @brief Initialize the queue and start the background worker thread
     * @param max_queue_size maximum queue capacity
     * @return true if success
     */
    bool initialize(size_t max_queue_size = 1000);
    
    /**
     * @brief Shutdown the queue and stop the worker thread
     */
    void shutdown();
    
    /**
     * @brief Enqueue a message
     * @param message the message to enqueue
     * @return true if added successfully, false if queue is full or not initialized
     */
    bool enqueue(const NotificationMessage& message);
    
    /**
     * @brief Convenience: enqueue a plain text message
     * @param content message content
     * @param type message type
     * @return true if enqueued successfully
     */
    bool sendMessage(const std::string& content, const std::string& type = "info");
    
    /**
     * @brief Register a notification sender implementation
     * @param sender sender implementation
     */
    void registerSender(std::shared_ptr<INotificationSender> sender);
    
    /**
     * @brief Get current queue size
     */
    size_t getQueueSize() const;
    
    /**
     * @brief Get count of sent messages
     */
    size_t getSentCount() const { return sent_count_; }
    
    /**
     * @brief Get count of failed messages
     */
    size_t getFailedCount() const { return failed_count_; }
    
    /**
     * @brief Wait until the queue is empty (for graceful shutdown)
     * @param timeout_seconds timeout in seconds
     * @return true if queue is empty before timeout
     */
    bool waitUntilEmpty(int timeout_seconds = 10);
    
    // Non-copyable and non-movable
    NotificationQueue(const NotificationQueue&) = delete;
    NotificationQueue& operator=(const NotificationQueue&) = delete;
    NotificationQueue(NotificationQueue&&) = delete;
    NotificationQueue& operator=(NotificationQueue&&) = delete;

private:
    NotificationQueue() = default;
    ~NotificationQueue();
    
    /**
     * @brief Background worker thread main function
     */
    void processingThread();
    
    /**
     * @brief Process a single message
     */
    void processMessage(const NotificationMessage& message);
    
    // Queue and thread management
    std::queue<NotificationMessage> message_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    std::atomic<bool> running_ = false;
    std::atomic<bool> initialized_ = false;
    std::unique_ptr<std::thread> worker_thread_;
    
    size_t max_queue_size_ = 1000;
    std::atomic<size_t> sent_count_ = 0;
    std::atomic<size_t> failed_count_ = 0;
    
    // Notification senders
    std::vector<std::shared_ptr<INotificationSender>> senders_;
    mutable std::mutex sender_mutex_;
};
