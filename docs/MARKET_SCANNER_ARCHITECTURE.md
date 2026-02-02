# Market Scanner - 架构设计文档

## 概述

参考Python代码（`opening_momentum_screener.py`）的设计模式，C++版本的Market Scanner实现了以下关键特性：

- **定时分批扫描**：避免一次性加载大量数据
- **接口抽象**：将数据获取逻辑与扫描逻辑解耦
- **时间敏感**：根据交易时段调整扫描频率
- **灵活筛选**：支持多维度评分和条件筛选

## 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                      MarketScanner                           │
│  (核心扫描逻辑、时间控制、筛选规则)                         │
└─────────────────┬───────────────────────────────────────────┘
                  │
                  │ 依赖
                  ▼
┌─────────────────────────────────────────────────────────────┐
│              IMarketDataProvider (接口)                      │
│  getMarketSnapshot() | getScannableStocks()                 │
└─────────────────┬───────────────────────────────────────────┘
                  │
         ┌────────┴────────┬──────────────┬──────────────┐
         │                 │              │              │
         ▼                 ▼              ▼              ▼
    ┌─────────┐    ┌──────────┐   ┌──────────┐   ┌──────────┐
    │  Futu   │    │Bloomberg │   │  Tiger   │   │  Others  │
    │Provider │    │ Provider │   │ Provider │   │Providers │
    └─────────┘    └──────────┘   └──────────┘   └──────────┘
    
    (各交易所具体实现)
```

## 组件说明

### 1. MarketScanner (市场扫描器)

**职责：**
- 定时扫描循环管理
- 分批数据获取调度
- 股票筛选和评分
- 结果处理和通知

**关键特性：**

#### 时间控制
```cpp
bool isInTradingTime()      // 检查是否在交易时段
bool isInOpeningPeriod()    // 检查是否在开盘期间
```

交易时段参考香港股市：
- 上午：9:30 - 12:00
- 下午：13:00 - 16:00
- 开盘期间：9:30 - 10:00（高频扫描）

#### 扫描间隔
- **开盘期间**：3000ms（3秒）- 捕捉追涨机会
- **正常时段**：5000ms（5秒）- 平衡反应速度和系统负载
- **非交易时段**：60000ms（60秒）- 低频检查

#### 分批获取
```cpp
static constexpr int BATCH_SIZE = 400;  // 每批400个股票
```

优势：
- 避免一次性加载数万个股票的数据
- 减少内存占用
- 实现流式处理
- 支持批次间延迟，避免API限流

### 2. IMarketDataProvider (数据提供者接口)

**定义数据获取的统一契约：**

```cpp
class IMarketDataProvider {
    // 批量获取市场快照
    virtual std::vector<ScanResult> getMarketSnapshot(
        const std::vector<std::string>& symbols) = 0;
    
    // 获取所有可扫描股票列表
    virtual std::vector<std::string> getScannableStocks() = 0;
};
```

**优点：**
- 新增交易所无需修改MarketScanner
- 便于单元测试（Mock实现）
- 支持多个数据源同时运行
- 符合依赖倒置原则

### 3. FutuDataProvider (Futu API实现示例)

参考实现，展示如何具体实现接口：

```cpp
class FutuDataProvider : public IMarketDataProvider {
public:
    bool initialize(const std::string& host, int port);
    std::vector<ScanResult> getMarketSnapshot(...) override;
    std::vector<std::string> getScannableStocks() override;
};
```

## 数据流

```
┌─────────────────────────────────────────────────────────────┐
│ MarketScanner::scanLoop()                                   │
│ (主循环)                                                    │
└────────────────┬────────────────────────────────────────────┘
                 │
                 ▼
        ┌────────────────────┐
        │ 检查是否交易时段   │
        └────────────────────┘
                 │
        ┌────────┴─────────┐
        │                  │
   是(交易中)           否(停机中)
        │                  │
        ▼                  ▼
    ┌────────────┐    ┌─────────────┐
    │ performScan│    │ sleep 60s   │
    └────────────┘    └─────────────┘
        │
        ▼
    ┌──────────────────────────────┐
    │ batchFetchMarketData()       │
    │ - 分批获取400个股票          │
    │ - 批次间隔300ms              │
    └──────────────────────────────┘
        │
        ▼
    ┌──────────────────────────────┐
    │ 计算评分                     │
    │ score = 40%涨幅 +           │
    │         30%换手率 +          │
    │         30%成交量            │
    └──────────────────────────────┘
        │
        ▼
    ┌──────────────────────────────┐
    │ 筛选合格股票                 │
    │ - 涨幅 1%-8%                 │
    │ - 换手率 >1%                 │
    │ - 价格 5-500元               │
    └──────────────────────────────┘
        │
        ▼
    ┌──────────────────────────────┐
    │ 排序取前10名                 │
    │ 发送给StrategyManager        │
    └──────────────────────────────┘
        │
        ▼
    ┌──────────────────────────────┐
    │ 根据时段选择下一次扫描间隔   │
    │ - 开盘期间：3秒              │
    │ - 正常时段：5秒              │
    └──────────────────────────────┘
```

## 筛选规则对比

### Python版本 vs C++版本

| 特性 | Python | C++ |
|------|--------|-----|
| 涨幅判断 | 连续上涨tick分析 | 简化为开盘价/昨收对比 |
| 量能分析 | 上涨tick占比 | 统一评分权重 |
| 时间控制 | 纳秒级精度 | 毫秒级精度 |
| 数据更新 | 实时websocket | 定时轮询 |
| 接口依赖 | 直接调用futu_client | 接口抽象 |

## 配置参数

在 `MarketScanner` 中定义的关键参数：

```cpp
// 分批处理
BATCH_SIZE = 400                    // 每批股票数量

// 扫描间隔（毫秒）
OPENING_SCAN_INTERVAL_MS = 3000     // 开盘期间
NORMAL_SCAN_INTERVAL_MS = 5000      // 正常时段
NON_TRADING_SCAN_INTERVAL_MS = 60000 // 非交易时段

// 筛选阈值
change_ratio: 0.01 - 0.08          // 涨幅1%-8%
turnover_rate: > 0.01              // 换手率>1%
price: 5.0 - 500.0                 // 价格范围

// 评分权重
change_ratio: 40%
turnover_rate: 30%
volume: 30%
```

## 线程安全

使用互斥锁保护共享资源：

```cpp
std::mutex watch_list_mutex_;       // 保护监控列表
std::mutex qualified_stocks_mutex_; // 保护合格股票列表
```

所有对共享资源的访问都通过 `std::lock_guard` 进行保护。

## 扩展指南

### 添加新交易所

1. **继承接口：**
```cpp
class MyExchangeProvider : public IMarketDataProvider {
public:
    std::vector<ScanResult> getMarketSnapshot(...) override;
    std::vector<std::string> getScannableStocks() override;
};
```

2. **实现数据获取：**
```cpp
std::vector<ScanResult> MyExchangeProvider::getMarketSnapshot(
    const std::vector<std::string>& symbols) {
    // 调用交易所API
    // 解析数据
    // 返回ScanResult列表
}
```

3. **集成到应用：**
```cpp
auto provider = std::make_shared<MyExchangeProvider>();
provider->initialize(...);
scanner.setDataProvider(provider);
```

### 自定义筛选规则

修改 `meetsSelectionCriteria()` 方法：

```cpp
bool MarketScanner::meetsSelectionCriteria(const ScanResult& result) const {
    // 添加自定义条件
    if (result.price < 2.0) return false;  // 股价不低于2元
    // ...
    return true;
}
```

### 调整评分算法

修改 `calculateScore()` 方法的权重：

```cpp
double MarketScanner::calculateScore(const ScanResult& result) const {
    double score = 0.0;
    score += result.change_ratio * 50.0;      // 涨幅权重提高到50%
    score += result.turnover_rate * 25.0;    // 换手率降低到25%
    // ...
    return score;
}
```

## 性能考虑

1. **内存使用**
   - 分批处理减少峰值内存占用
   - 每次扫描结果在处理后释放

2. **CPU使用**
   - 非交易时段低频扫描
   - 线程安全的互斥锁最小化锁竞争

3. **网络带宽**
   - 批次间延迟300ms
   - 可配置扫描间隔

4. **API调用限制**
   - 每批400个股票
   - 遵守交易所API限流策略

## 日志输出

使用 `LOG_INFO` 和 `LOG_ERROR` 进行系统日志记录：

```
Market scanner initialized
FutuDataProvider connected to 127.0.0.1:11111
Market scanner started
Loaded 1000 stocks for scanning
Starting market scan...
Scan completed: found 3 qualified stocks
```

## 参考

- Python版本：`opening_momentum_screener.py`
- 扫描结果数据结构：`ScanResult` in strategy_manager.h
- 策略管理器：`StrategyManager::processScanResults()`
