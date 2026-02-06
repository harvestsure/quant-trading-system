#include "notification/notification_queue.h"
#include "utils/logger.h"
#include <chrono>
#include <sstream>
#include <iomanip>

NotificationQueue& NotificationQueue::getInstance() {
    static NotificationQueue instance;
    return instance;
}

NotificationQueue::~NotificationQueue() {
    shutdown();
}

bool NotificationQueue::initialize(size_t max_queue_size) {
    if (initialized_.exchange(true)) {
        LOG_WARN("NotificationQueue is already initialized");
        return true;
    }
    
    max_queue_size_ = max_queue_size;
    running_ = true;
    worker_thread_ = std::make_unique<std::thread>(&NotificationQueue::processingThread, this);
    
    LOG_INFO("NotificationQueue initialized with max_queue_size: " + std::to_string(max_queue_size_));
    return true;
}

void NotificationQueue::shutdown() {
    if (!initialized_) {
        return;
    }
    
    LOG_INFO("Shutting down NotificationQueue...");
    
    // 停止接收新消息
    running_ = false;
    
    // 唤醒处理线程
    queue_cv_.notify_one();
    
    // 等待处理线程完成
    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
        LOG_INFO("NotificationQueue worker thread stopped");
    }
    
    // 处理剩余的消息
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!message_queue_.empty()) {
            auto message = message_queue_.front();
            message_queue_.pop();
            processMessage(message);
        }
    }
    
    initialized_ = false;
}

bool NotificationQueue::enqueue(const NotificationMessage& message) {
    if (!running_) {
        LOG_WARN("NotificationQueue is not running");
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        if (message_queue_.size() >= max_queue_size_) {
            LOG_WARN("NotificationQueue is full, dropping message");
            failed_count_++;
            return false;
        }
        
        message_queue_.push(message);
    }
    
    queue_cv_.notify_one();
    return true;
}

bool NotificationQueue::sendMessage(const std::string& content, const std::string& type) {
    NotificationMessage msg(content, type);
    
    // 生成唯一ID
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&timestamp), "%Y%m%d%H%M%S");
    msg.id = ss.str() + "_" + std::to_string(reinterpret_cast<uintptr_t>(this));
    
    // 设置时间戳（毫秒）
    msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    return enqueue(msg);
}

void NotificationQueue::registerSender(std::shared_ptr<INotificationSender> sender) {
    if (!sender) {
        LOG_WARN("Attempting to register a null sender");
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(sender_mutex_);
        senders_.push_back(sender);
    }
    
    LOG_INFO("Notification sender registered, total senders: " + std::to_string(senders_.size()));
}

size_t NotificationQueue::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return message_queue_.size();
}

bool NotificationQueue::waitUntilEmpty(int timeout_seconds) {
    auto deadline = std::chrono::steady_clock::now() + 
                    std::chrono::seconds(timeout_seconds);
    
    while (true) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (message_queue_.empty()) {
                return true;
            }
        }
        
        if (std::chrono::steady_clock::now() >= deadline) {
            LOG_WARN("Timeout waiting for queue to empty");
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void NotificationQueue::processingThread() {
    LOG_INFO("NotificationQueue processing thread started");
    
    while (running_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        // 等待消息或shutdown信号
        queue_cv_.wait(lock, [this] { 
            return !message_queue_.empty() || !running_; 
        });
        
        if (!message_queue_.empty()) {
            auto message = message_queue_.front();
            message_queue_.pop();
            
            // 释放锁，以便在处理消息时其他线程可以添加新消息
            lock.unlock();
            
            processMessage(message);
            
            lock.lock();
        }
    }
    
    LOG_INFO("NotificationQueue processing thread stopped");
}

void NotificationQueue::processMessage(const NotificationMessage& message) {
    std::lock_guard<std::mutex> lock(sender_mutex_);
    
    bool success = false;
    for (auto& sender : senders_) {
        if (sender && sender->isReady()) {
            try {
                if (sender->send(message)) {
                    success = true;
                    LOG_DEBUG("Message sent via sender: " + std::string(typeid(*sender).name()));
                } else {
                    LOG_WARN("Sender failed to send message: " + std::string(typeid(*sender).name()));
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Exception while sending message: " + std::string(e.what()));
            }
        }
    }
    
    if (success) {
        sent_count_++;
        LOG_DEBUG(std::string("Message processed successfully, ID: ") + message.id);
    } else {
        failed_count_++;
        LOG_WARN(std::string("Failed to send message via any sender, ID: ") + message.id);
    }
}
