#include "event/event_engine.h"
#include "utils/logger.h"
#include <sstream>

EventEngine& EventEngine::getInstance() {
    static EventEngine instance;
    return instance;
}

EventEngine::~EventEngine() {
    stop();
}

void EventEngine::start() {
    if (running_) {
        LOG_WARN("EventEngine is already running");
        return;
    }
    
    running_ = true;
    
    // Start event processing thread
    event_thread_ = std::make_unique<std::thread>(&EventEngine::eventLoop, this);
    
    LOG_INFO("EventEngine started");
}

void EventEngine::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Notify event loop thread to exit
    queue_cv_.notify_all();
    
    // Wait for thread to finish
    if (event_thread_ && event_thread_->joinable()) {
        event_thread_->join();
    }
    
    std::stringstream ss;
    ss << "EventEngine stopped. Total processed events: " << processed_count_;
    LOG_INFO(ss.str());
}

int EventEngine::registerHandler(EventType type, EventHandler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    
    int handler_id = next_handler_id_++;
    handlers_[type][handler_id] = handler;
    
    std::stringstream ss;
    ss << "Registered handler #" << handler_id << " for event type: " << eventTypeToString(type);
    LOG_INFO(ss.str());
    
    return handler_id;
}

void EventEngine::unregisterHandler(EventType type, int handler_id) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    
    auto it = handlers_.find(type);
    if (it != handlers_.end()) {
        it->second.erase(handler_id);
        
        if (it->second.empty()) {
            handlers_.erase(it);
        }
        
        std::stringstream ss;
        ss << "Unregistered handler #" << handler_id << " for event type: " << eventTypeToString(type);
        LOG_INFO(ss.str());
    }
}

void EventEngine::putEvent(const EventPtr& event) {
    if (!event) {
        LOG_WARN("Attempted to put null event");
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        event_queue_.push(event);
    }
    
    // Notify event processing thread
    queue_cv_.notify_one();
}

void EventEngine::eventLoop() {
    while (running_) {
        EventPtr event;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            // Wait for event or stop signal
            queue_cv_.wait(lock, [this] {
                return !event_queue_.empty() || !running_;
            });
            
            // If stopped and queue is empty, exit
            if (!running_ && event_queue_.empty()) {
                break;
            }
            
            // Get event from queue
            if (!event_queue_.empty()) {
                event = event_queue_.front();
                event_queue_.pop();
            }
        }
        
        // Process event
        if (event) {
            processEvent(event);
            processed_count_++;
        }
    }
}

void EventEngine::processEvent(const EventPtr& event) {
    std::vector<EventHandler> handlers_to_call;
    
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        
        auto it = handlers_.find(event->getType());
        if (it != handlers_.end()) {
            // Copy handler list to avoid deadlock from modifying handler map in callback
            for (const auto& pair : it->second) {
                handlers_to_call.push_back(pair.second);
            }
        }
    }
    
    // Call handlers
    for (const auto& handler : handlers_to_call) {
        try {
            handler(event);
        } catch (const std::exception& e) {
            std::stringstream ss;
            ss << "Exception in event handler for " << eventTypeToString(event->getType())
               << ": " << e.what();
            LOG_ERROR(ss.str());
        } catch (...) {
            std::stringstream ss;
            ss << "Unknown exception in event handler for " << eventTypeToString(event->getType());
            LOG_ERROR(ss.str());
        }
    }
}

size_t EventEngine::getEventQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return event_queue_.size();
}

size_t EventEngine::getHandlerCount(EventType type) const {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    
    auto it = handlers_.find(type);
    return it != handlers_.end() ? it->second.size() : 0;
}

