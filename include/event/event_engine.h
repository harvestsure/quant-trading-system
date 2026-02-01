#pragma once

#include "event.h"
#include "event_type.h"
#include <functional>
#include <map>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

 

// 事件处理器类型定义
using EventHandler = std::function<void(const EventPtr&)>;

// 事件引擎
class EventEngine {
public:
    static EventEngine& getInstance();
    
    // 启动/停止事件引擎
    void start();
    void stop();
    bool isRunning() const { return running_; }
    
    // 注册事件处理器
    // 返回处理器ID，用于后续注销
    int registerHandler(EventType type, EventHandler handler);
    
    // 注销事件处理器
    void unregisterHandler(EventType type, int handler_id);
    
    // 发布事件
    void putEvent(const EventPtr& event);
    
    // 便捷方法：创建并发布事件
    template<typename T>
    void publishEvent(EventType type, const T& data) {
        auto event = std::make_shared<Event>(type);
        event->setData(data);
        putEvent(event);
    }
    
    // 获取统计信息
    size_t getEventQueueSize() const;
    size_t getHandlerCount(EventType type) const;
    uint64_t getProcessedEventCount() const { return processed_count_; }
    
    // 禁止拷贝
    EventEngine(const EventEngine&) = delete;
    EventEngine& operator=(const EventEngine&) = delete;
    
private:
    EventEngine() = default;
    ~EventEngine();
    
    // 事件处理线程
    void eventLoop();
    
    // 处理单个事件
    void processEvent(const EventPtr& event);
    
    // 事件队列
    std::queue<EventPtr> event_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // 事件处理器映射：事件类型 -> <处理器ID, 处理器>
    std::map<EventType, std::map<int, EventHandler>> handlers_;
    mutable std::mutex handlers_mutex_;
    
    // 处理器ID生成器
    std::atomic<int> next_handler_id_{0};
    
    // 事件处理线程
    std::unique_ptr<std::thread> event_thread_;
    std::atomic<bool> running_{false};
    
    // 统计信息
    std::atomic<uint64_t> processed_count_{0};
};

 
