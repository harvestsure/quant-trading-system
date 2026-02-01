# Futu量化交易系统

一个基于C++和Futu API的量化交易系统，支持实盘和模拟盘交易，实现追涨杀跌策略。

## 功能特性

- ✅ **双模式交易**：支持实盘和模拟盘交易
- ✅ **市场扫描**：定时5分钟扫描港股市场，筛选优秀个股
- ✅ **策略系统**：灵活的策略管理器，支持多种策略并行运行
- ✅ **配置管理**：从配置文件读取所有参数
- ✅ **数据订阅**：实时订阅K线和Ticker数据
- ✅ **持仓管理**：自动跟踪和管理所有持仓
- ✅ **风险控制**：完善的风险管理系统，包括止损、止盈、仓位控制

## 系统架构

```
quant-trading-system/
├── include/               # 头文件
│   ├── config/           # 配置管理
│   ├── managers/         # 核心管理器
│   ├── scanner/          # 市场扫描器
│   ├── data/             # 数据订阅
│   ├── trading/          # 交易执行
│   ├── strategies/       # 交易策略
│   └── utils/            # 工具类
├── src/                  # 源文件
├── config.json            # 配置文件
└── CMakeLists.txt        # 构建配置
```

### 核心模块

1. **ConfigManager**：配置管理器，从文件读取所有配置
2. **PositionManager**：持仓管理器，跟踪所有持仓状态
3. **RiskManager**：风险管理器，控制风险和仓位
4. **StrategyManager**：策略管理器，管理多个策略实例
5. **MarketScanner**：市场扫描器，定时扫描市场寻找机会
6. **DataSubscriber**：数据订阅器，订阅实时行情数据
7. **OrderExecutor**：订单执行器，处理下单和订单管理

## 编译和安装

### 前置要求

- C++17或更高版本
- CMake 3.15或更高版本
- Futu OpenD（已配置好）
- Futu API库（需要放在 `futu-api/` 目录）
- `nlohmann/json`（项目使用 git 子模块存放在 `libraries/json`，CMake 会优先使用该子模块）

### 获取依赖（git 子模块）

项目现在使用 `nlohmann/json` 作为子模块（位于 `libraries/json`）。第一次检出仓库后执行：

```bash
git submodule update --init --recursive
```

如果你还没有添加子模块（手工操作），可以运行：

```bash
git submodule add https://github.com/nlohmann/json.git libraries/json
git submodule update --init --recursive
```

### 编译步骤（建议使用项目脚本）

项目根提供了 `build.sh` 简化构建流程。示例：

```bash
chmod +x build.sh
./build.sh --enable-futu
```

在 macOS 上，`nproc` 可能不可用；`build.sh` 已做兼容处理（会回退到 `sysctl -n hw.ncpu`）。如果你手动使用 `make`，也请先创建 `build` 目录并运行 `cmake ..`。

```bash
# 手动构建示例
mkdir -p build
cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

## 配置说明

编辑 `config.json` 文件配置系统参数：

```ini
# 交易模式
is_simulation=true              # true=模拟盘, false=实盘

# Futu OpenD连接配置
futu_host=127.0.0.1            # OpenD服务器地址
futu_port=11111                # OpenD端口
unlock_password=               # 解锁密码（可选）

# 资金管理
max_position_size=100000.0     # 最大持仓金额
single_stock_max_ratio=0.2     # 单只股票最大占比20%
max_positions=10               # 最多持仓数量

# 扫描参数
scan_interval_minutes=5        # 扫描间隔（分钟）
market=HK                      # 市场代码

# 风险管理
stop_loss_ratio=0.05          # 止损比例5%
take_profit_ratio=0.15        # 止盈比例15%
max_daily_loss=0.03           # 每日最大亏损3%
```

## 使用说明

### 1. 启动系统

```bash
./quant-trading-system config.json
```

### 2. 系统工作流程

1. **启动**：系统加载配置，连接Futu OpenD
2. **扫描**：每5分钟扫描港股市场，筛选符合条件的股票
3. **分析**：策略接收扫描结果，进行技术分析
4. **交易**：满足条件时自动下单买入
5. **监控**：实时监控持仓，触发止损/止盈时自动卖出
6. **报告**：每分钟打印系统状态和持仓信息

### 3. 停止系统

按 `Ctrl+C` 优雅退出系统。

## 策略说明

### 动量追涨策略 (MomentumStrategy)

**核心思想**：追踪强势上涨的股票，在趋势确认后买入。

**入场条件**：
- 股票处于上升趋势（价格在20日均线上方）
- RSI在30-70之间（避免超买超卖）
- 涨幅在2%-6%之间
- 换手率大于2%（有足够流动性）

**出场条件**：
- 触发止损（亏损5%）
- 触发止盈（盈利15%）
- 趋势反转

**技术指标**：
- RSI（相对强弱指标）
- MA20（20日移动平均线）
- 成交量分析

## 开发自定义策略

继承 `StrategyBase` 类创建自己的策略：

```cpp
#include "strategies/strategy_base.h"

class MyStrategy : public StrategyBase {
public:
    MyStrategy() : StrategyBase("MyStrategy") {}
    
    void onScanResult(const ScanResult& result) override {
        // 处理扫描结果
        if (/* 你的条件 */) {
            // 订阅数据
            subscribeStock(result.symbol);
            
            // 下单
            buy(result.symbol, quantity, price);
        }
    }
    
    void onKLine(const std::string& symbol, const KLine& kline) override {
        // 处理K线数据
        // 实现你的交易逻辑
    }
};
```

在 `main.cpp` 中添加你的策略：

```cpp
auto my_strategy = std::make_shared<MyStrategy>();
strategy_mgr.addStrategy(my_strategy);
```

## 风险控制

系统内置多层风险控制：

1. **仓位控制**
   - 单只股票最多占总资金20%
   - 最多同时持有10只股票
   - 总资金不超过配置的最大值

2. **止损止盈**
   - 自动止损：亏损5%
   - 自动止盈：盈利15%

3. **每日风控**
   - 每日最大亏损限制3%
   - 触发后停止当日交易

4. **订单风控**
   - 每笔订单前检查资金充足性
   - 检查持仓数量限制
   - 检查单股占比限制

## 日志系统

系统会自动记录所有操作到 `trading_system.log`：

```
2025-01-15 10:00:00.123 [INFO] System started
2025-01-15 10:05:00.456 [INFO] Market scan completed: found 5 stocks
2025-01-15 10:05:01.789 [INFO] Order placed: HK.00700 BUY 200 @ 350.0
2025-01-15 10:10:00.234 [WARNING] Stop loss triggered for HK.00700
```

## 注意事项

⚠️ **重要提示**：

1. **先用模拟盘**：充分测试后再使用实盘
2. **Futu API配置**：确保OpenD正常运行并配置好
3. **代码集成**：需要将Futu API的实际调用代码集成到标注了`TODO`的位置
4. **资金安全**：合理设置风险参数，不要超出承受范围
5. **监控系统**：运行时持续监控系统状态和日志

## TODO集成清单

系统框架已完成，以下位置需要集成真实的Futu API调用：

1. `src/scanner/market_scanner.cpp` - 市场扫描API调用
2. `src/data/data_subscriber.cpp` - 数据订阅API调用
3. `src/trading/order_executor.cpp` - 下单API调用
4. `src/main.cpp` - API连接和初始化

## 性能监控

系统每分钟输出状态报告：

```
========== System Status ==========
Active Strategies: 1
Total Positions: 3
Total Market Value: $52,500
Total P/L: $2,500
Daily P/L: $1,200 (2.4%)
Total Trades: 15 (Win: 9, Loss: 6)

--- Current Positions ---
HK.00700: 200 @ $350.0 (Current: $355.0, P/L: $1000 1.43%)
HK.00939: 500 @ $80.0 (Current: $82.0, P/L: $1000 2.50%)
HK.01810: 100 @ $120.0 (Current: $125.0, P/L: $500 4.17%)
===================================
```

## 许可证

本项目仅供学习和研究使用。

## 联系方式

如有问题或建议，请创建Issue。

---

**风险提示**：股市有风险，投资需谨慎。本系统仅供参考，使用者需自行承担交易风险。
