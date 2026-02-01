# 动态策略管理机制

## 概述

本系统实现了基于市场扫描结果的动态策略实例管理机制。策略实例会根据股票是否符合扫描条件自动创建和删除，并在删除前智能判断是否有持仓。

## 工作流程

### 1. 扫描阶段

市场扫描器（MarketScanner）每5分钟执行一次扫描：

```
定时扫描 → 获取港股市场数据 → 应用筛选条件 → 评分排序 → 选出前10名
```

**筛选条件**（追涨杀跌策略）：
- 涨幅：1% - 8% 之间（强势但不极端）
- 换手率：> 1%（有足够流动性）
- 价格：5元 - 500元（合理价格区间）

**评分算法**：
- 涨幅权重：40%
- 换手率权重：30%
- 成交量权重：30%

### 2. 策略管理阶段

策略管理器（StrategyManager）接收扫描结果后，执行以下逻辑：

#### 2.1 创建新策略实例

```cpp
for (const auto& result : scan_results) {
    if (!hasStrategyInstance(result.symbol)) {
        // 股票代码不存在 → 创建新策略实例
        createStrategyInstance(result.symbol, result);
    } else {
        // 股票代码已存在 → 更新扫描结果
        updateExistingStrategy(result.symbol, result);
    }
}
```

**创建过程**：
1. 检查策略实例是否已存在（避免重复创建）
2. 创建对应的策略对象（默认为MomentumStrategy）
3. 启动策略实例
4. 传递扫描结果给策略
5. 记录到策略实例映射表

#### 2.2 删除不符合条件的策略实例

```cpp
for (const auto& symbol : not_in_current_scan) {
    removeStrategyInstance(symbol, force=false);
}
```

**删除逻辑**：

```
是否在最新扫描结果中？
    ├─ 是 → 保留策略实例
    └─ 否 → 进入删除判断
             │
             └─ 是否有持仓？
                 ├─ 是 → 不删除，标记为INACTIVE，继续监控
                 └─ 否 → 删除策略实例
```

**关键保护机制**：
- `canRemoveStrategy()` 方法会检查持仓管理器
- 有持仓的股票不会删除策略，而是标记为不活跃状态
- 不活跃策略仍会运行，继续监控持仓状态
- 等待平仓后才会在下次扫描时删除

### 3. 策略实例状态

每个策略实例有两个状态标志：

```cpp
struct StrategyInstance {
    std::string symbol;           // 股票代码
    std::shared_ptr<StrategyBase> strategy;  // 策略对象
    bool is_active;                   // 是否活跃
};
```

**状态说明**：
- `is_active = true`：股票在最新扫描结果中，正常运行
- `is_active = false`：股票不在扫描结果中，但有持仓，继续监控

## 使用示例

### 场景1：新股票符合条件

```
时间 T0: 扫描发现 HK.00700（腾讯）符合条件
        → 创建策略实例 "Momentum_HK.00700"
        → 策略开始订阅数据、分析并交易

时间 T1: 下次扫描，HK.00700 仍符合条件
        → 策略实例已存在，不重复创建
        → 更新最新扫描结果给策略
```

### 场景2：股票不再符合条件（无持仓）

```
时间 T0: HK.00700 在扫描结果中，有策略实例

时间 T1: HK.00700 不在扫描结果中
        → 检查持仓管理器：无持仓
        → 停止并删除策略实例 "Momentum_HK.00700"
```

### 场景3：股票不再符合条件（有持仓）

```
时间 T0: HK.00700 在扫描结果中，有策略实例，已建仓

时间 T1: HK.00700 不在扫描结果中
        → 检查持仓管理器：有持仓
        → 不删除策略实例
        → 标记为 is_active = false
        → 策略继续运行，监控持仓，等待止盈/止损

时间 T2: 触发止损，平仓完成
        → HK.00700 仍不在扫描结果中
        → 检查持仓管理器：无持仓
        → 删除策略实例
```

## 并发安全

所有策略管理操作都通过互斥锁保护：

```cpp
std::lock_guard<std::mutex> lock(mutex_);
```

确保在多线程环境下：
- 扫描线程安全地通知策略管理器
- 策略实例的创建/删除不会冲突
- 持仓检查的原子性

## 配置参数

在 `config.json` 中可以调整以下参数：

```
scan_interval_minutes=5    # 扫描间隔（分钟）
max_positions=10           # 最大持仓数量
max_position_size=10000    # 单个持仓最大金额
```

## 日志输出

系统会记录所有策略生命周期事件：

```
[INFO] Created strategy instance for HK.00700 (腾讯控股) - Score: 2.35, Price: 350.0, Change: 2.5%
[WARNING] Cannot remove strategy for HK.00700 - has active position, will keep monitoring
[INFO] Removed strategy instance for HK.00700
```

## 扩展开发

### 添加自定义策略类型

1. 继承 `StrategyBase` 创建新策略类
2. 在 `StrategyManager::createStrategy()` 中添加策略选择逻辑

```cpp
std::shared_ptr<StrategyBase> StrategyManager::createStrategy(
    const std::string& symbol, 
    const ScanResult& scan_result) {
    
    // 根据条件选择策略类型
    if (scan_result.volatility > 0.05) {
        return std::make_shared<HighVolatilityStrategy>();
    } else {
        return std::make_shared<MomentumStrategy>();
    }
}
```

### 自定义删除条件

修改 `StrategyManager::canRemoveStrategy()` 方法：

```cpp
bool StrategyManager::canRemoveStrategy(const std::string& symbol) const {
    // 检查持仓
    if (PositionManager::getInstance().hasPosition(symbol)) {
        return false;
    }
    
    // 检查挂单
    if (OrderManager::getInstance().hasPendingOrder(symbol)) {
        return false;
    }
    
    // 其他自定义条件...
    
    return true;
}
```

## 性能考虑

- 策略实例使用智能指针管理，自动内存回收
- 使用 `std::map` 实现 O(log n) 的查找复杂度
- 扫描结果使用集合比对，避免 O(n²) 遍历
- 持仓检查委托给持仓管理器，支持并发访问

## 注意事项

1. **持仓保护**：有持仓的策略不会被删除，确保交易安全
2. **策略延续**：标记为不活跃的策略仍会继续监控，等待平仓
3. **线程安全**：所有公共方法都是线程安全的
4. **资源管理**：策略实例使用智能指针，无需手动释放
5. **日志追踪**：所有关键操作都有日志记录，便于调试

## 监控命令

运行时系统会每分钟输出状态：

```
========== System Status ==========
Active Strategies: 8
Strategy Instances: HK.00700, HK.09988, HK.01810, ...
Total Positions: 5
...
===================================
```

也可以在代码中调用：

```cpp
StrategyManager::getInstance().printStrategyStatus();
```
