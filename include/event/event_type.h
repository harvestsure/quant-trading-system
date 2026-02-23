#pragma once

#include <string>

 

// Event type definitions
enum class EventType {
    // Market data events
    EVENT_TICK,              // Tick data
    EVENT_KLINE,             // Kline data
    EVENT_DEPTH,             // Market depth
    EVENT_TRADE,             // Trade-by-trade
    
    // Trading events
    EVENT_ORDER,             // Order status update
    EVENT_TRADE_DEAL,        // Trade execution report
    EVENT_POSITION,          // Position update
    EVENT_ACCOUNT,           // Account update
    
    // System events
    EVENT_LOG,               // Log event
    EVENT_ERROR,             // Error event
    EVENT_TIMER,             // Timer event
    
    // Strategy events
    EVENT_STRATEGY_START,    // Strategy start
    EVENT_STRATEGY_STOP,     // Strategy stop
    EVENT_SIGNAL,            // Trading signal
    
    // Scan events
    EVENT_SCAN_RESULT        // Scan result
};

// Event type to string conversion
inline std::string eventTypeToString(EventType type) {
    switch (type) {
        case EventType::EVENT_TICK: return "EVENT_TICK";
        case EventType::EVENT_KLINE: return "EVENT_KLINE";
        case EventType::EVENT_DEPTH: return "EVENT_DEPTH";
        case EventType::EVENT_TRADE: return "EVENT_TRADE";
        case EventType::EVENT_ORDER: return "EVENT_ORDER";
        case EventType::EVENT_TRADE_DEAL: return "EVENT_TRADE_DEAL";
        case EventType::EVENT_POSITION: return "EVENT_POSITION";
        case EventType::EVENT_ACCOUNT: return "EVENT_ACCOUNT";
        case EventType::EVENT_LOG: return "EVENT_LOG";
        case EventType::EVENT_ERROR: return "EVENT_ERROR";
        case EventType::EVENT_TIMER: return "EVENT_TIMER";
        case EventType::EVENT_STRATEGY_START: return "EVENT_STRATEGY_START";
        case EventType::EVENT_STRATEGY_STOP: return "EVENT_STRATEGY_STOP";
        case EventType::EVENT_SIGNAL: return "EVENT_SIGNAL";
        case EventType::EVENT_SCAN_RESULT: return "EVENT_SCAN_RESULT";
        default: return "UNKNOWN";
    }
}

 
