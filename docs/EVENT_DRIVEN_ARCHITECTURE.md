# 事件驱动架构设计文档

## 概述

本系统采用类似vnpy的事件驱动架构，实现了完全解耦的模块间通信机制。所有数据流动通过事件引擎进行分发，模块间不直接调用，而是通过发布-订阅模式进行通信。

## 核心组件

### 1. 事件引擎 (EventEngine)

**位置**: `include/event/event_engine.h`, `src/event/event_engine.cpp`

事件引擎是整个系统的消息中枢，负责：
- 维护事件队列
- 管理事件处理器（订阅者）
- 异步分发事件到对应的处理器
- 线程安全的事件处理

**核心方法**:
```cpp
// 启动/停止事件引擎
void start();
void stop();

// 注册事件处理器（订阅事件）
int registerHandler(EventType type, EventHandler handler);

// 发布事件
void putEvent(const EventPtr& event);

// 便捷方法：创建并发布事件
template<typename T>
void publishEvent(EventType type, const T& data);
```

**工作流程**:
1. 系统启动时启动事件引擎
2. 各模块注册自己感兴趣的事件类型
3. 数据源（如交易所）发布事件到队列
4. 事件线程异步取出事件并分发给所有订阅者
5. 系统关闭时停止事件引擎

### 2. 事件类型 (EventType)

**位置**: `include/event/event_type.h`

定义了系统中所有的事件类型：

**行情事件**:
- `EVENT_TICK`: Tick数据更新
- `EVENT_KLINE`: K线数据更新
- `EVENT_DEPTH`: 深度行情更新
- `EVENT_TRADE`: 逐笔成交数据

**交易事件**:
- `EVENT_ORDER`: 订单状态更新
- `EVENT_TRADE_DEAL`: 成交回报
- `EVENT_POSITION`: 持仓更新
- `EVENT_ACCOUNT`: 账户资金更新

**策略事件**:
- `EVENT_STRATEGY_START`: 策略启动
- `EVENT_STRATEGY_STOP`: 策略停止
- `EVENT_SIGNAL`: 交易信号

**系统事件**:
- `EVENT_LOG`: 日志事件
- `EVENT_ERROR`: 错误事件
- `EVENT_TIMER`: 定时器事件
- `EVENT_SCAN_RESULT`: 市场扫描结果

### 3. 统一数据格式 (EventData)

**位置**: `#include "common/object.h`

所有交易所的原始数据都会被转换为统一的数据格式，确保策略代码与具体交易所解耦。

**核心数据结构**:

```cpp
// Tick数据（实时行情）
struct TickData {
    std::string symbol;           // 股票代码
    std::string exchange;         // 交易所
    int64_t timestamp;            // 时间戳
    double last_price;            // 最新价
    int64_t volume;               // 成交量
    double bid_price_1;           // 买一价
    // ... 更多字段
};

// K线数据
struct KlineData {
    std::string symbol;
    std::string exchange;
    int64_t timestamp;
    std::string interval;         // K线周期
    double open_price;
    double high_price;
    double low_price;
    double close_price;
    int64_t volume;
};

// 订单数据
struct OrderData {
    std::string order_id;
    std::string symbol;
    std::string exchange;
    Direction direction;          // 买/卖
    OrderType type;              // 市价/限价
    OrderStatus status;          // 订单状态
    double price;
    int64_t volume;
    int64_t traded_volume;
};

// 成交数据
struct TradeData {
    std::string trade_id;
    std::string order_id;
    std::string symbol;
    Direction direction;
    double price;
    int64_t volume;
};

// 持仓数据
struct PositionData {
    std::string symbol;
    Direction direction;
    int64_t volume;
    double avg_price;
    double current_price;
    double profit_loss;
};

// 账户数据
struct AccountData {
    std::string account_id;
    double balance;
    double available;
    double market_value;
};
```

## 数据流转示例

### 场景1: Tick数据流转

```
Futu API回调 
    ↓
FutuExchange::onTickUpdate(futu_tick)
    ↓
转换为TickData（统一格式）
    ↓
EventEngine::publishEvent(EVENT_TICK, tick_data)
    ↓
事件队列 → 事件循环线程
    ↓
分发给所有订阅者:
    - Strategy::onTick(tick_data)  [策略更新]
    - DataRecorder::onTick(tick_data)  [数据记录]
    - RiskManager::onTick(tick_data)  [风控监控]
```

### 场景2: 订单状态更新

```
Futu API订单回调
    ↓
FutuExchange::onOrderUpdate(futu_order)
    ↓
转换为OrderData（统一格式）
    ↓
EventEngine::publishEvent(EVENT_ORDER, order_data)
    ↓
分发给订阅者:
    - Strategy::onOrder(order_data)  [策略处理]
    - PositionManager::onOrder(order_data)  [更新持仓]
    - RiskManager::onOrder(order_data)  [风险检查]
```

### 场景3: K线数据流转

```
Futu API K线推送
    ↓
FutuExchange::onKLineUpdate(futu_kline)
    ↓
转换为KlineData（统一格式）
    ↓
EventEngine::publishEvent(EVENT_KLINE, kline_data)
    ↓
分发给订阅者:
    - Strategy::onKline(kline_data)  [策略计算信号]
    - Indicator::onKline(kline_data)  [更新技术指标]
```

## 交易所接口实现

### Futu交易所事件发布

**位置**: `src/exchange/futu_exchange.cpp`

交易所实现类负责：
1. 接收交易所原始数据
2. 转换为统一数据格式
3. 发布事件到事件引擎

**示例代码**:
```cpp
void FutuExchange::publishTickEvent(const std::string& symbol, const void* futu_tick) {
    // 1. 转换Futu原始数据为统一格式
    TickData tick_data;
    tick_data.symbol = symbol;
    tick_data.exchange = "Futu";
    tick_data.timestamp = getCurrentTimestamp();
    // ... 从futu_tick提取并填充数据
    
    // 2. 发布到事件引擎
    auto& event_engine = EventEngine::getInstance();
    event_engine.publishEvent(EventType::EVENT_TICK, tick_data);
}

void FutuExchange::publishKLineEvent(const std::string& symbol, const void* futu_kline) {
    KlineData kline_data;
    kline_data.symbol = symbol;
    kline_data.exchange = "Futu";
    // ... 填充数据
    
    auto& event_engine = EventEngine::getInstance();
    event_engine.publishEvent(EventType::EVENT_KLINE, kline_data);
}
```

## 策略订阅事件

### 策略基类中的事件处理

**位置**: 需要在`StrategyBase`中添加事件处理方法

```cpp
class StrategyBase {
public:
    void subscribeEvents() {
        auto& event_engine = EventEngine::getInstance();
        
        // 订阅Tick事件
        tick_handler_id_ = event_engine.registerHandler(
            EventType::EVENT_TICK,
            [this](const EventPtr& event) {
                if (auto* tick = event->getData<TickData>()) {
                    if (tick->symbol == stock_code_) {
                        onTick(*tick);
                    }
                }
            }
        );
        
        // 订阅K线事件
        kline_handler_id_ = event_engine.registerHandler(
            EventType::EVENT_KLINE,
            [this](const EventPtr& event) {
                if (auto* kline = event->getData<KlineData>()) {
                    if (kline->symbol == stock_code_) {
                        onKline(*kline);
                    }
                }
            }
        );
        
        // 订阅订单事件
        order_handler_id_ = event_engine.registerHandler(
            EventType::EVENT_ORDER,
            [this](const EventPtr& event) {
                if (auto* order = event->getData<OrderData>()) {
                    if (order->strategy_name == name_) {
                        onOrder(*order);
                    }
                }
            }
        );
    }
    
    void unsubscribeEvents() {
        auto& event_engine = EventEngine::getInstance();
        event_engine.unregisterHandler(EventType::EVENT_TICK, tick_handler_id_);
        event_engine.unregisterHandler(EventType::EVENT_KLINE, kline_handler_id_);
        event_engine.unregisterHandler(EventType::EVENT_ORDER, order_handler_id_);
    }

protected:
    // 子类重写这些方法处理具体事件
    virtual void onTick(const TickData& tick) {}
    virtual void onKline(const KlineData& kline) {}
    virtual void onOrder(const OrderData& order) {}

private:
    int tick_handler_id_ = -1;
    int kline_handler_id_ = -1;
    int order_handler_id_ = -1;
};
```

## 添加新交易所的步骤

当需要集成IBKR、Binance等新交易所时：

### 1. 创建交易所实现类

```cpp
class IBKRExchange : public IExchange {
public:
    // 实现基类的所有虚函数
    bool subscribeKLine(...) override;
    bool subscribeTick(...) override;
    
private:
    // 事件发布方法
    void publishTickEvent(const std::string& symbol, const void* ibkr_tick);
    void publishKLineEvent(const std::string& symbol, const void* ibkr_kline);
};
```

### 2. 实现数据转换

```cpp
void IBKRExchange::publishTickEvent(const std::string& symbol, const void* ibkr_tick) {
    // 将IBKR原始数据转换为TickData
    TickData tick_data;
    tick_data.symbol = symbol;
    tick_data.exchange = "IBKR";
    // ... 从ibkr_tick中提取数据
    
    // 发布事件（与Futu完全相同）
    auto& event_engine = EventEngine::getInstance();
    event_engine.publishEvent(EventType::EVENT_TICK, tick_data);
}
```

### 3. 策略无需修改

因为所有数据都转换为统一格式，策略代码完全不需要修改！策略只需要：
- 订阅`EVENT_TICK`
- 接收`TickData`（统一格式）
- 无需关心数据来自Futu还是IBKR

## 优势

### 1. 完全解耦
- 策略与交易所解耦
- 模块间通过事件通信，无直接依赖
- 易于测试和维护

### 2. 易于扩展
- 添加新交易所只需实现接口
- 添加新策略只需订阅事件
- 添加新功能模块只需订阅相关事件

### 3. 数据统一
- 所有交易所数据转换为统一格式
- 策略代码可跨交易所复用
- 简化了策略开发

### 4. 异步非阻塞
- 事件异步处理，不阻塞数据接收
- 多个订阅者并行处理事件
- 提高系统性能

### 5. 线程安全
- 事件引擎内部处理线程同步
- 模块无需关心多线程问题
- 降低开发复杂度

## 系统启动流程

```cpp
int main() {
    // 1. 启动事件引擎（必须最先启动）
    auto& event_engine = EventEngine::getInstance();
    event_engine.start();
    
    // 2. 初始化交易所
    auto& exchange_mgr = ExchangeManager::getInstance();
    exchange_mgr.initExchange(ExchangeType::FUTU, config);
    exchange_mgr.connect();
    
    // 3. 初始化策略管理器
    auto& strategy_mgr = StrategyManager::getInstance();
    // 策略实例在创建时会自动订阅事件
    
    // 4. 启动市场扫描器
    MarketScanner scanner;
    scanner.start();
    
    // 5. 系统运行...
    
    // 6. 停止顺序（反向）
    scanner.stop();
    strategy_mgr.stopAllStrategies();
    exchange_mgr.disconnect();
    event_engine.stop();  // 最后停止
}
```

## 性能考虑

### 事件队列大小
- 默认无上限，可配置最大队列长度
- 队列满时可选择丢弃或阻塞

### 处理器执行时间
- 事件处理器应尽快返回
- 耗时操作应异步执行
- 避免在处理器中进行阻塞调用

### 事件过滤
- 在处理器中进行必要的过滤（如股票代码、策略名称）
- 减少不必要的处理

## 调试和监控

```cpp
// 获取事件引擎统计信息
auto& event_engine = EventEngine::getInstance();
size_t queue_size = event_engine.getEventQueueSize();
size_t handler_count = event_engine.getHandlerCount(EventType::EVENT_TICK);
uint64_t processed = event_engine.getProcessedEventCount();

LOG_INFO("Event Queue Size: " + std::to_string(queue_size));
LOG_INFO("Tick Handlers: " + std::to_string(handler_count));
LOG_INFO("Total Processed: " + std::to_string(processed));
```

## 最佳实践

1. **事件优先级**: 关键事件（如订单、成交）优先处理
2. **错误处理**: 事件处理器中捕获异常，避免影响其他订阅者
3. **日志记录**: 关键事件发布和处理都应记录日志
4. **性能监控**: 监控事件队列长度和处理延迟
5. **资源清理**: 模块销毁时记得注销事件处理器

## 总结

事件驱动架构使系统具有高度的模块化和可扩展性，是构建复杂量化交易系统的理想架构。通过统一的数据格式和解耦的模块设计，系统可以轻松支持多个交易所，策略代码可以无缝迁移，极大地提高了开发效率和代码质量。
