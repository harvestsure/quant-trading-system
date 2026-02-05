#pragma once

#include "event_type.h"
#include <functional>
#include <memory>
#include <cstddef>
#include <cstdint>

// 前向声明
class Event;
using EventPtr = std::shared_ptr<Event>;

// 事件处理器类型定义
using EventHandler = std::function<void(const EventPtr&)>;

// 事件引擎基类接口
class IEventEngine {
public:
    virtual ~IEventEngine() = default;
    
    // 启动/停止事件引擎
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    
    // 注册事件处理器
    virtual int registerHandler(EventType type, EventHandler handler) = 0;
    
    // 注销事件处理器
    virtual void unregisterHandler(EventType type, int handler_id) = 0;
    
    // 发布事件
    virtual void putEvent(const EventPtr& event) = 0;
    
    // 获取统计信息
    virtual size_t getEventQueueSize() const = 0;
    virtual size_t getHandlerCount(EventType type) const = 0;
    virtual uint64_t getProcessedEventCount() const = 0;
};
