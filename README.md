# 量化交易系统

一个基于C++的多交易所量化交易系统，支持Futu、IBKR、Binance等多个交易平台，支持实盘和模拟盘交易，实现追涨杀跌策略。

## 功能特性

- ✅ **多交易所支持**：支持Futu、IBKR、Binance等多个交易平台
- ✅ **双模式交易**：支持实盘和模拟盘交易
- ✅ **市场扫描**：定时扫描市场，筛选符合条件的交易机会
- ✅ **策略系统**：灵活的策略管理器，支持多种策略并行运行
- ✅ **配置管理**：JSON配置文件，支持多个交易所配置
- ✅ **数据订阅**：实时订阅K线和Ticker数据
- ✅ **持仓管理**：自动跟踪和管理所有持仓
- ✅ **风险控制**：完善的风险管理系统，包括止损、止盈、追踪止损、仓位控制

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
- `nlohmann/json`（项目使用 git 子模块存放在 `libraries/json`，CMake 会优先使用该子模块）
- 根据使用的交易所选择对应的API库：
  - **Futu**：FTAPI4CPP（需要单独下载配置）
  - **IBKR**：Interactive Brokers TWS API
  - **Binance**：Binance 官方API（需要API Key和Secret）

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

### 配置 Futu API（FTAPI4CPP）

如果需要启用 Futu 交易所支持，需要先配置 FTAPI4CPP 库。

#### 第一步：获取 FTAPI4CPP

从 Futu 官网下载 FTAPI4CPP（例如版本 9.6.5608 或其他版本），解压到本地目录，目录结构应该如下：

```
FTAPI4CPP_<version>/
├── Include/          # 头文件目录
│   ├── FTAPI.h
│   ├── FTSPI.h
│   ├── FTAPIChannel.h
│   └── ...
└── Bin/              # 库文件目录
    ├── Mac/
    │   ├── Release/
    │   │   ├── libFTAPI.a
    │   │   ├── libprotobuf.a
    │   │   └── libFTAPIChannel.dylib
    │   └── Debug/
    ├── Ubuntu16.04/
    ├── Centos7/
    └── ...
```

#### 第二步：编译时指定 FTAPI_HOME

有 **3 种方式** 配置 FTAPI 路径：

**方式一：命令行参数（推荐）**

```bash
chmod +x build.sh
./build.sh --ftapi-home /path/to/FTAPI4CPP_<version> --debug
```

**方式二：环境变量**

```bash
export FTAPI_HOME=/path/to/FTAPI4CPP_<version>
./build.sh --debug
```

**方式三：CMake 直接调用**

```bash
mkdir -p build
cd build
cmake -DFTAPI_HOME=/path/to/FTAPI4CPP_<version> -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)  # macOS 使用：make -j$(sysctl -n hw.ncpu)
```

### 编译步骤

#### 使用构建脚本（推荐）

```bash
# 启用 Futu 交易所，Release 模式
./build.sh --ftapi-home /path/to/FTAPI4CPP_<version>

# 启用 Futu 交易所，Debug 模式
./build.sh --ftapi-home /path/to/FTAPI4CPP_<version> --debug

# 禁用 Futu，启用 IBKR
./build.sh --disable-futu --enable-ibkr

# 查看所有可用选项
./build.sh --help
```

#### 手动编译

```bash
mkdir -p build
cd build
cmake -DFTAPI_HOME=/path/to/FTAPI4CPP_<version> -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)  # macOS
# 或
make -j$(nproc)  # Linux
```

#### 构建脚本参数说明

```
选项说明:
  --enable-futu              启用 Futu 交易所支持（默认：ON）
  --disable-futu             禁用 Futu 交易所支持
  --enable-ibkr              启用 IBKR 交易所支持（默认：OFF）
  --enable-binance           启用 Binance 交易所支持（默认：OFF）
  --ftapi-home <path>        指定 FTAPI4CPP 主目录
  --debug                    Debug 模式编译
  --release                  Release 模式编译（默认）
  --help                     显示帮助信息
```

## 配置说明

编辑 `config.json` 文件配置系统参数。系统采用JSON格式配置，支持多个交易所：

### 基础配置

```json
{
  "exchange": {
    "type": "FUTU",           // 交易所类型：FUTU、IBKR、BINANCE
    "is_simulation": true      // true=模拟盘, false=实盘
  }
}
```

### 交易所连接配置

**Futu配置**：
```json
"futu": {
  "host": "127.0.0.1",        // OpenD服务器地址
  "port": 11111,               // OpenD端口
  "unlock_password": "",       // 解锁密码（可选）
  "market": "HK"               // 市场代码：HK、US等
}
```

**IBKR配置**：
```json
"ibkr": {
  "host": "127.0.0.1",        // TWS服务器地址
  "port": 7496,                // TWS端口
  "client_id": 0,              // 客户端ID
  "account": ""                // 账户ID
}
```

**Binance配置**：
```json
"binance": {
  "api_key": "",              // API Key
  "api_secret": "",           // API Secret
  "testnet": true              // true=测试网, false=正式网
}
```

### 资金管理

```json
"trading": {
  "max_position_size": 100000.0,    // 最大持仓金额
  "single_stock_max_ratio": 0.2,    // 单只股票最大占比20%
  "max_positions": 10               // 最多同时持仓数量
}
```

### 市场扫描参数

```json
"scanner": {
  "interval_minutes": 5,       // 扫描间隔（分钟）
  "min_price": 1.0,            // 最低价格
  "max_price": 1000.0,         // 最高价格
  "min_volume": 1000000,       // 最小成交量
  "min_turnover_rate": 0.01,   // 最小换手率
  "top_n": 10                  // 返回前N个候选股票
}
```

### 风险管理

```json
"risk": {
  "stop_loss_ratio": 0.05,      // 止损比例5%
  "take_profit_ratio": 0.15,    // 止盈比例15%
  "max_daily_loss": 0.03,       // 每日最大亏损3%
  "trailing_stop_ratio": 0.03,  // 追踪止损比例3%
  "max_drawdown": 0.1           // 最大回撤限制10%
}
```

### 策略参数

```json
"strategy": {
  "momentum": {
    "enabled": true,           // 是否启用动量策略
    "rsi_period": 14,          // RSI周期
    "rsi_oversold": 30,        // RSI超卖阈值
    "rsi_overbought": 70,      // RSI超买阈值
    "ma_period": 20,           // 移动平均线周期
    "volume_factor": 1.5       // 成交量系数
  }
}
```

### 日志配置

```json
"logging": {
  "level": "INFO",            // 日志级别：DEBUG、INFO、WARNING、ERROR
  "console": true,             // 是否输出到控制台
  "file": true,                // 是否输出到文件
  "file_path": "logs/trading.log"  // 日志文件路径
}
```

## 使用说明

### 1. 启动系统

```bash
./quant-trading-system config.json
```

### 2. 系统工作流程

1. **启动**：系统加载配置，连接到选定的交易所（Futu/IBKR/Binance）
2. **扫描**：定时扫描市场，筛选符合条件的交易机会
3. **分析**：策略接收扫描结果，进行技术分析
4. **交易**：满足条件时自动下单执行
5. **监控**：实时监控持仓，触发止损/止盈/追踪止损时自动平仓
6. **报告**：定期输出系统状态和持仓信息

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
2. **交易所配置**：根据使用的交易所正确配置连接参数和凭证
3. **API集成**：确保已正确集成所选交易所的API库
4. **资金安全**：合理设置风险参数，不要超出承受范围
5. **监控系统**：运行时持续监控系统状态和日志
6. **权限设置**：确保API凭证具有所需的交易权限

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
