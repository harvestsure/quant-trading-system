# 交易所抽象层设计文档

## 概述

本系统采用抽象接口设计，将交易所相关功能封装在统一的接口层，方便未来集成多个交易所（如 IBKR、Binance 等），而无需修改核心业务逻辑代码。

## 架构设计

```
┌─────────────────────────────────────────────┐
│          应用层 (Main / Strategies)          │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│         ExchangeManager (统一入口)           │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│         IExchange (抽象接口)                 │
└──────┬──────────┬──────────┬────────────────┘
       │          │          │
  ┌────▼───┐ ┌───▼────┐ ┌───▼──────┐
  │  Futu  │ │  IBKR  │ │ Binance  │
  │Exchange│ │Exchange│ │ Exchange │
  └────────┘ └────────┘ └──────────┘
```

## 核心组件

### 1. IExchange (交易所接口基类)

定义所有交易所必须实现的标准接口：

**连接管理**
- `connect()` - 连接到交易所
- `disconnect()` - 断开连接
- `isConnected()` - 检查连接状态
- `getType()` - 获取交易所类型
- `getName()` - 获取交易所名称

**账户管理**
- `getAccountInfo()` - 获取账户信息
- `getPositions()` - 获取持仓列表
- `getAvailableFunds()` - 获取可用资金

**交易功能**
- `placeOrder()` - 下单
- `cancelOrder()` - 撤单
- `modifyOrder()` - 改单
- `getOrderStatus()` - 查询订单状态
- `getOrderHistory()` - 获取历史订单

**行情数据**
- `subscribeKLine()` / `unsubscribeKLine()` - 订阅/取消K线
- `subscribeTick()` / `unsubscribeTick()` - 订阅/取消Tick
- `getHistoryKLine()` - 获取历史K线
- `getSnapshot()` - 获取快照数据
- `getBatchSnapshots()` - 批量获取快照

**市场扫描**
- `getMarketStockList()` - 获取市场股票列表
- `getBatchSnapshots()` - 批量获取行情快照

**回调注册**
- `registerOrderUpdateCallback()` - 订单更新回调
- `registerKLineCallback()` - K线数据回调
- `registerTickCallback()` - Tick数据回调
- `registerSnapshotCallback()` - 快照数据回调

### 2. FutuExchange (Futu实现)

Futu富途证券的具体实现类：

```cpp
class FutuExchange : public IExchange {
public:
    explicit FutuExchange(const FutuConfig& config);
    
    // 实现所有IExchange接口
    bool connect() override;
    std::string placeOrder(...) override;
    // ... 其他接口实现
    
private:
    FutuConfig config_;
    bool connected_;
    
    // Futu API相关内部方法
    bool initFutuAPI();
    bool unlockTrade();
    std::string convertStockCode(const std::string& symbol);
    
    // 数据转换方法
    Order convertFutuOrder(const void* futu_order);
    KLine convertFutuKLine(const void* futu_kline);
    // ...
};
```

**配置参数：**
```cpp
struct FutuConfig {
    std::string host = "127.0.0.1";
    int port = 11111;
    std::string unlock_password = "";
    bool is_simulation = true;
    std::string market = "HK";
};
```

### 3. ExchangeFactory (交易所工厂)

负责根据交易所类型创建具体实例：

```cpp
std::shared_ptr<IExchange> ExchangeFactory::createExchange(
    ExchangeType type,
    const std::map<std::string, std::string>& config
);
```

**支持的交易所类型：**
- `ExchangeType::FUTU` - Futu富途证券 ✅ 已实现
- `ExchangeType::IBKR` - Interactive Brokers 🚧 待实现
- `ExchangeType::BINANCE` - Binance币安 🚧 待实现

### 4. ExchangeManager (交易所管理器)

系统访问交易所的统一单例入口：

```cpp
// 初始化交易所
ExchangeManager& mgr = ExchangeManager::getInstance();
mgr.initExchange(ExchangeType::FUTU, config);
mgr.connect();

// 使用交易所功能
AccountInfo account = mgr.getAccountInfo();
std::string order_id = mgr.placeOrder("00700", "BUY", 100, "MARKET");
std::vector<KLine> klines = mgr.getHistoryKLine("00700", "K_5M", 100);
```

## 配置文件

在 `config.json` 中设置交易所类型和连接参数，示例：

```json
{
    "exchange": {
        "type": "FUTU",
        "is_simulation": true
    },
    "futu": {
        "host": "127.0.0.1",
        "port": 11111,
        "unlock_password": ""
    }
}
```

## 使用示例

### 1. 系统启动时初始化交易所

```cpp
// main.cpp
auto& config_mgr = ConfigManager::getInstance();
config_mgr.loadFromFile("config.json");
const auto& config = config_mgr.getConfig();

// 转换交易所类型
ExchangeType exchange_type = ExchangeType::FUTU;
if (config.exchange_type == "IBKR") {
    exchange_type = ExchangeType::IBKR;
}

// 准备配置
std::map<std::string, std::string> exchange_config;
exchange_config["host"] = config.futu_host;
exchange_config["port"] = std::to_string(config.futu_port);
exchange_config["is_simulation"] = config.is_simulation ? "true" : "false";

// 初始化并连接
auto& exchange_mgr = ExchangeManager::getInstance();
exchange_mgr.initExchange(exchange_type, exchange_config);
exchange_mgr.connect();
```

### 2. 策略中使用交易所

```cpp
// 策略类中
#include "exchange/exchange_manager.h"

void MomentumStrategy::executeOrder(const std::string& symbol) {
    auto& exchange_mgr = ExchangeManager::getInstance();
    
    // 检查可用资金
    double funds = exchange_mgr.getAvailableFunds();
    
    // 下单
    std::string order_id = exchange_mgr.placeOrder(
        symbol,
        "BUY",
        100,
        "MARKET"
    );
    
    // 订阅数据
    exchange_mgr.subscribeKLine(symbol, "K_5M");
}
```

### 3. 数据订阅和回调

```cpp
// 注册K线数据回调
exchange_mgr.registerKLineCallback(
    [](const std::string& symbol, const KLine& kline) {
        // 处理K线数据
        std::cout << "Received KLine: " << symbol 
                  << " close=" << kline.close << std::endl;
    }
);

// 订阅K线
exchange_mgr.subscribeKLine("00700", "K_5M");
```

## 添加新交易所

### 步骤 1: 创建实现类

```cpp
// include/exchange/ibkr_exchange.h
#pragma once
#include "exchange_interface.h"

struct IBKRConfig {
    std::string host;
    int port;
    int client_id;
    // ...
};

class IBKRExchange : public IExchange {
public:
    explicit IBKRExchange(const IBKRConfig& config);
    
    // 实现所有IExchange接口
    bool connect() override {
        // IBKR连接逻辑
        // ...
    }
    
    std::string placeOrder(...) override {
        // IBKR下单逻辑
        // ...
    }
    
    // ... 实现其他所有接口
    
private:
    IBKRConfig config_;
    // IBKR API相关成员
};
```

### 步骤 2: 在工厂中注册

```cpp
// src/exchange/exchange_factory.cpp
case ExchangeType::IBKR: {
    IBKRConfig ibkr_config;
    
    // 从config读取参数
    if (config.find("host") != config.end()) {
        ibkr_config.host = config.at("host");
    }
    // ...
    
    return std::make_shared<IBKRExchange>(ibkr_config);
}
```

### 步骤 3: 更新配置

将运行时配置切换到 `config.json`，示例：

```json
{
    "exchange": { "type": "IBKR" },
    "ibkr": {
        "host": "127.0.0.1",
        "port": 7497,
        "client_id": 1
    }
}
```

### 步骤 4: 实现数据转换

```cpp
// IBKRExchange内部方法
Order IBKRExchange::convertIBKROrder(const IBOrder* ibkr_order) {
    Order order;
    order.order_id = std::to_string(ibkr_order->orderId);
    order.symbol = ibkr_order->symbol;
    // ... 转换其他字段
    return order;
}
```

## 数据结构映射

### 通用订单结构

```cpp
struct Order {
    std::string order_id;        // 订单ID
    std::string symbol;      // 股票代码
    OrderType type;              // MARKET / LIMIT
    OrderSide side;              // BUY / SELL
    int quantity;                // 数量
    double price;                // 价格
    OrderStatus status;          // 状态
    int filled_quantity;         // 已成交数量
    double filled_price;         // 成交均价
    std::string create_time;     // 创建时间
    std::string update_time;     // 更新时间
};
```

### 通用K线结构

```cpp
struct KLine {
    std::string time;    // 时间戳
    double open;         // 开盘价
    double high;         // 最高价
    double low;          // 最低价
    double close;        // 收盘价
    double volume;       // 成交量
    double turnover;     // 成交额
};
```

### 账户信息结构

```cpp
struct AccountInfo {
    std::string account_id;      // 账户ID
    double total_assets;         // 总资产
    double cash;                 // 现金
    double market_value;         // 市值
    double available_funds;      // 可用资金
    double frozen_funds;         // 冻结资金
    std::string currency;        // 货币
};
```

## 优势

### 1. 解耦合
- 核心业务逻辑与交易所实现完全分离
- 策略代码不需要关心底层交易所细节

### 2. 可扩展
- 添加新交易所只需实现接口，无需修改现有代码
- 支持多交易所同时运行（未来扩展）

### 3. 可测试
- 可以创建Mock交易所进行单元测试
- 模拟盘和实盘切换简单

### 4. 灵活配置
- 通过配置文件切换交易所
- 无需重新编译代码

## 注意事项

### 1. 股票代码格式
不同交易所的股票代码格式可能不同：
- Futu: "HK.00700" (需要市场前缀)
- IBKR: "700.HK" (不同格式)
- 需要在实现类中处理转换

### 2. 数据类型转换
- 各交易所返回的数据结构不同
- 需要在实现类中转换为统一格式

### 3. 时区处理
- 不同交易所可能使用不同时区
- 建议统一使用UTC时间

### 4. 错误处理
- 各交易所的错误码不同
- 需要统一错误处理机制

### 5. 回调线程安全
- 交易所回调可能在不同线程
- 需要注意线程安全和锁的使用

## 未来扩展

### 1. 多交易所支持
```cpp
// 同时使用多个交易所
ExchangeManager& futu_mgr = ExchangeManager::getInstance("futu");
ExchangeManager& ibkr_mgr = ExchangeManager::getInstance("ibkr");

futu_mgr.placeOrder("00700", "BUY", 100, "MARKET");  // Futu下单
ibkr_mgr.placeOrder("AAPL", "BUY", 10, "MARKET");    // IBKR下单
```

### 2. 交易所路由
根据股票自动选择最佳交易所：
```cpp
class ExchangeRouter {
public:
    std::shared_ptr<IExchange> selectExchange(const std::string& symbol);
};
```

### 3. 统一订单簿
合并多个交易所的订单和持仓：
```cpp
class UnifiedOrderBook {
public:
    std::vector<Order> getAllOrders();  // 来自所有交易所
    std::vector<Position> getAllPositions();
};
```

## 总结

交易所抽象层为系统提供了强大的扩展能力，使得添加新交易所变得简单直接。核心设计原则是"面向接口编程"，确保系统的灵活性和可维护性。
