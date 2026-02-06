# 消息通知队列系统

本文档描述了基于Telegram Bot的消息通知队列系统的架构和使用方式。

## 系统概述

消息通知队列系统是一个高性能、可扩展的异步消息处理系统，设计用于将交易系统中的关键事件（如交易信号、订单成交、风险警告等）实时推送到Telegram。

### 核心特性

- **异步处理**：采用后台线程处理消息，不阻塞主交易逻辑
- **队列缓存**：支持配置队列大小，防止消息丢失
- **多发送器支持**：可注册多个消息发送器，支持同时向多个渠道发送
- **可扩展设计**：通过实现`INotificationSender`接口可轻松添加新的发送器（邮件、SMS等）
- **错误恢复**：完整的日志记录和错误统计

## 架构设计

```
┌─────────────────────────────────────────────────────────┐
│                  Trading Core                            │
│  (Strategies, OrderExecutor, RiskManager, etc.)         │
└────────────────────┬────────────────────────────────────┘
                     │
         ┌───────────┴───────────┐
         │                       │
    sendMessage()         registerSender()
         │                       │
         v                       v
┌─────────────────────────────────────────────────────────┐
│           NotificationManager (Singleton)               │
│  - Initialize from config                              │
│  - Convenience methods (sendTradeSignal, etc.)         │
└────────────────────┬────────────────────────────────────┘
                     │
                     v
┌─────────────────────────────────────────────────────────┐
│         NotificationQueue (Singleton)                   │
│  - enqueue(message) - Main entry point                 │
│  - processingThread() - Background worker               │
│  - Statistics (sent_count, failed_count)               │
└────────────────────┬────────────────────────────────────┘
                     │
         ┌───────────┴───────────────────────┐
         │                                   │
    send(message)                   send(message)
         │                                   │
         v                                   v
┌──────────────────────┐          ┌──────────────────────┐
│   TelegramSender     │          │  CustomSender        │
│  (INotificationSender)│         │ (INotificationSender) │
│ - HTTP POST via      │          │                      │
│   cpp-httplib        │          │ - Email, SMS, etc.  │
│ - Telegram Bot API   │          │                      │
└──────────────────────┘          └──────────────────────┘
```

## 核心组件

### 1. NotificationMessage

```cpp
struct NotificationMessage {
    std::string id;           // 消息唯一ID
    std::string content;      // 消息内容
    std::string type;         // 消息类型
    int64_t timestamp;        // 时间戳（毫秒）
    int retry_count = 0;      // 重试次数
};
```

**消息类型示例**：
- `"trade"` - 交易执行消息
- `"trade_signal"` - 交易信号
- `"error"` - 错误警告
- `"info"` - 一般信息

### 2. INotificationSender

抽象接口，定义消息发送器的行为：

```cpp
class INotificationSender {
public:
    virtual bool send(const NotificationMessage& message) = 0;
    virtual bool isReady() const = 0;
};
```

### 3. NotificationQueue

核心消息队列类，管理消息的入队、缓存和分发：

**关键方法**：
- `initialize(size_t max_queue_size)` - 初始化队列
- `enqueue(const NotificationMessage&)` - 入队消息
- `registerSender(std::shared_ptr<INotificationSender>)` - 注册发送器
- `getQueueSize()` - 获取当前队列大小
- `waitUntilEmpty(int timeout_seconds)` - 等待队列清空

### 4. TelegramSender

基于cpp-httplib的Telegram Bot消息发送器：

**特性**：
- 使用HTTPS安全连接到Telegram API
- 支持消息格式化（时间戳、消息类型等）
- 自动处理HTTP超时
- `testConnection()` 方法验证Bot Token和Chat ID

### 5. NotificationManager

高级管理器，简化系统初始化和使用：

```cpp
// 初始化
NotificationManager& notif = NotificationManager::getInstance();
notif.initialize();  // 从全局配置读取

// 便捷方法
notif.sendTradeSignal("BUY XYZ");
notif.sendTradeExecution("Order filled");
notif.sendError("Risk limit exceeded");
notif.sendInfo("System started");

// 优雅关闭
notif.waitUntilEmpty(10);
notif.shutdown();
```

## 配置文件

在 `config.json` 中的 `notification.telegram` 部分：

```json
{
  "notification": {
    "telegram": {
      "enabled": true,
      "bot_token": "YOUR_BOT_TOKEN_HERE",
      "chat_id": "YOUR_CHAT_ID_HERE",
      "api_timeout_seconds": 5,
      "max_queue_size": 1000,
      "batch_send": false,
      "batch_size": 10,
      "batch_interval_ms": 1000
    }
  }
}
```

### 配置参数详解

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| enabled | bool | false | 是否启用Telegram通知 |
| bot_token | string | "" | Telegram Bot Token（从 @BotFather 获取） |
| chat_id | string | "" | 目标Chat ID（可以是个人或群组） |
| api_timeout_seconds | int | 5 | HTTP请求超时时间 |
| max_queue_size | int | 1000 | 消息队列最大容量 |
| batch_send | bool | false | 是否启用批量发送（预留，暂未实现） |
| batch_size | int | 10 | 批量发送的批大小 |
| batch_interval_ms | int | 1000 | 批量发送的间隔（毫秒） |

## 使用示例

### 基本集成

```cpp
#include "notification/notification_manager.h"
#include "config/config_manager.h"

int main() {
    // 加载配置
    ConfigManager::getInstance().loadFromJson("config.json");
    
    // 初始化通知系统
    NotificationManager& notif = NotificationManager::getInstance();
    if (!notif.initialize()) {
        LOG_ERROR("Failed to initialize notifications");
        return 1;
    }
    
    // 发送消息
    notif.sendInfo("Trading system initialized");
    
    // ... 交易逻辑 ...
    
    // 优雅关闭
    notif.waitUntilEmpty(10);
    notif.shutdown();
    
    return 0;
}
```

### 在策略中使用

```cpp
#include "notification/notification_manager.h"

void MomentumStrategy::onSignal(const SignalEvent& event) {
    std::string message = event.symbol + ": " + 
                         (event.is_buy ? "BUY" : "SELL") + 
                         " signal at " + std::to_string(event.price);
    
    NotificationManager::getInstance().sendTradeSignal(message);
}
```

### 在订单执行模块中使用

```cpp
void OrderExecutor::onOrderFilled(const Order& order) {
    std::string message = "✅ " + order.symbol + 
                         " " + std::to_string(order.quantity) +
                         " shares @ " + std::to_string(order.filled_price);
    
    NotificationManager::getInstance().sendTradeExecution(message);
}
```

### 在风险管理中使用

```cpp
void RiskManager::checkDailyLoss() {
    if (today_loss > max_daily_loss) {
        NotificationManager::getInstance().sendError(
            "⚠️ Daily loss limit exceeded: " + std::to_string(today_loss)
        );
    }
}
```

## 获取Telegram Bot信息

### 步骤1：创建Bot（获取bot_token）

1. 在Telegram中搜索 `@BotFather`
2. 发送命令 `/newbot`
3. 按照提示输入Bot名称和用户名
4. BotFather返回格式如下的Token：
   ```
   123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11
   ```

### 步骤2：获取Chat ID

**方法A - 个人聊天：**

1. 直接与你的Bot聊天，发送任意消息
2. 在浏览器中访问：
   ```
   https://api.telegram.org/bot<YOUR_BOT_TOKEN>/getUpdates
   ```
3. 在JSON响应中找到 `"chat":{"id": 123456789}`
4. 那个ID就是你的chat_id

**方法B - 群组（用于团队通知）：**

1. 创建一个私有群组
2. 将Bot添加到群组
3. 在群组中发送一条消息
4. 使用方法A获取群组的chat_id（具体是负数，例如 `-1001234567890`）

## 运行和调试

### 编译

```bash
cd /Users/sure/Code/quant-trading-system
rm -rf build && mkdir build
cd build
cmake ..
make
```

### 测试配置

```bash
# 修改 config.json
{
  "notification": {
    "telegram": {
      "enabled": true,
      "bot_token": "你的token",
      "chat_id": "你的chat_id"
    }
  }
}

# 运行程序
./build/quant-trading-system
```

### 查看日志

系统会在 `logs/` 目录下生成详细日志，包括：
- 消息队列操作
- 发送器状态和连接
- 每条消息的发送结果

查找相关日志：
```bash
grep "TelegramSender\|NotificationQueue\|NotificationManager" logs/quant-trading-system.log
```

## 错误处理

### 常见问题

1. **"Invalid bot token"**
   - 检查token格式是否正确
   - 确保从 @BotFather 复制完整

2. **"Invalid chat ID"**
   - 确保chat_id正确（可通过getUpdates验证）
   - 群组chat_id需要是负数

3. **"Connection timeout"**
   - 检查网络连接
   - 调整 `api_timeout_seconds` 参数
   - 检查防火墙设置

4. **消息未送达**
   - 检查Bot是否在对应的chat中有权限
   - 查看日志中的失败原因
   - 确实已启用通知（checked `enabled: true`）

## 扩展指南

### 添加新的消息发送器（如邮件）

```cpp
#include "notification/notification_queue.h"

class EmailSender : public INotificationSender {
public:
    bool send(const NotificationMessage& message) override {
        // 实现邮件发送逻辑
        return true;
    }
    
    bool isReady() const override {
        return !email_.empty() && !smtp_server_.empty();
    }
    
private:
    std::string email_;
    std::string smtp_server_;
};

// 在NotificationManager初始化中注册
auto email_sender = std::make_shared<EmailSender>(config);
queue.registerSender(email_sender);
```

### 自定义消息类型

在消息发送时使用自定义类型：

```cpp
NotificationQueue::getInstance().sendMessage(
    "Custom message content",
    "custom_type"
);
```

然后在接收端（如Telegram格式化器）处理这个类型。

## 性能考虑

- **队列大小**：默认1000条消息。高频交易系统可能需要增加
- **线程消耗**：后台只有一个处理线程，性能开销最小
- **HTTP延迟**：Telegram API响应通常< 500ms，不会阻塞主线程
- **内存使用**：单条消息约200-500字节

## 线程安全

所有公共API都是线程安全的：
- 消息队列使用mutex保护
- 发送器列表有独立的锁
- 统计计数器使用atomic

可以从多个线程安全地调用 `sendMessage()`。
