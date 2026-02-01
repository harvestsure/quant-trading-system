#pragma once

#include <string>

 

// 事件类型定义
enum class EventType {
    // 行情事件
    EVENT_TICK,              // Tick数据
    EVENT_KLINE,             // K线数据
    EVENT_DEPTH,             // 深度行情
    EVENT_TRADE,             // 逐笔成交
    
    // 交易事件
    EVENT_ORDER,             // 订单状态更新
    EVENT_TRADE_DEAL,        // 成交回报
    EVENT_POSITION,          // 持仓更新
    EVENT_ACCOUNT,           // 账户资金更新
    
    // 系统事件
    EVENT_LOG,               // 日志事件
    EVENT_ERROR,             // 错误事件
    EVENT_TIMER,             // 定时器事件
    
    // 策略事件
    EVENT_STRATEGY_START,    // 策略启动
    EVENT_STRATEGY_STOP,     // 策略停止
    EVENT_SIGNAL,            // 交易信号
    
    // 扫描事件
    EVENT_SCAN_RESULT        // 扫描结果
};

// 事件类型转字符串
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

 
