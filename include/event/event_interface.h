#pragma once

#include "event_type.h"
#include <functional>
#include <memory>
#include <cstddef>
#include <cstdint>

// Forward declarations
class Event;
using EventPtr = std::shared_ptr<Event>;

// Event handler type definition
using EventHandler = std::function<void(const EventPtr&)>;

// Event engine base interface
class IEventEngine {
public:
    virtual ~IEventEngine() = default;
    
    // Start/stop the event engine
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    
    // Register event handler
    virtual int registerHandler(EventType type, EventHandler handler) = 0;
    
    // Unregister event handler
    virtual void unregisterHandler(EventType type, int handler_id) = 0;
    
    // Publish event
    virtual void putEvent(const EventPtr& event) = 0;
    
    // Get statistics
    virtual size_t getEventQueueSize() const = 0;
    virtual size_t getHandlerCount(EventType type) const = 0;
    virtual uint64_t getProcessedEventCount() const = 0;
};
