#pragma once

#include "event.h"
#include "event_interface.h"
#include <map>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

 

// Event engine
class EventEngine : public IEventEngine {
public:
    static EventEngine& getInstance();
    
    // Start/stop the event engine
    void start() override;
    void stop() override;
    bool isRunning() const override { return running_; }
    
    // Register event handler
    // Returns handler ID for later unregistration
    int registerHandler(EventType type, EventHandler handler) override;
    
    // Unregister event handler
    void unregisterHandler(EventType type, int handler_id) override;
    
    // Publish event
    void putEvent(const EventPtr& event) override;
    
    // Convenience: create and publish an event
    template<typename T>
    void publishEvent(EventType type, const T& data) {
        auto event = std::make_shared<Event>(type);
        event->setData(data);
        putEvent(event);
    }
    
    // Get statistics
    size_t getEventQueueSize() const override;
    size_t getHandlerCount(EventType type) const override;
    uint64_t getProcessedEventCount() const override { return processed_count_; }
    
    // Non-copyable
    EventEngine(const EventEngine&) = delete;
    EventEngine& operator=(const EventEngine&) = delete;
    
private:
    EventEngine() = default;
    ~EventEngine();
    
    // Event handling thread
    void eventLoop();
    
    // Process a single event
    void processEvent(const EventPtr& event);
    
    // Event queue
    std::queue<EventPtr> event_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // Handler map: EventType -> <handlerID, handler>
    std::map<EventType, std::map<int, EventHandler>> handlers_;
    mutable std::mutex handlers_mutex_;
    
    // Handler ID generator
    std::atomic<int> next_handler_id_{0};
    
    // Event processing thread
    std::unique_ptr<std::thread> event_thread_;
    std::atomic<bool> running_{false};
    
    // Statistics
    std::atomic<uint64_t> processed_count_{0};
};

 
