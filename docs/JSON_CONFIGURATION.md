# JSON配置系统

## 概述

系统已升级为使用JSON格式的配置文件，相比之前的`key=value`文本格式，JSON配置提供了更强大、更灵活的配置管理方式。

## 为什么使用JSON？

## 配置格式

系统现在只支持 JSON 格式的配置文件。请使用 `config.json`（或任意以 `.json` 结尾的文件名）来提供运行时配置，示例如下：

```json
{
  "exchange": {
    "type": "FUTU",
    "is_simulation": true
  },
  "futu": {
    "host": "127.0.0.1",
    "port": 11111
  }
}
```
{ 
  "exchange": {
    "type": "FUTU",
    "is_simulation": true
  },
  
  "futu": {
    "host": "127.0.0.1",
    "port": 11111,
    "unlock_password": "",
    "market": "HK"
  },
  
  "ibkr": {
    "host": "127.0.0.1",
    "port": 7496,
    "client_id": 0,
    "account": ""
  },
  
  "binance": {
    "api_key": "",
    "api_secret": "",
    "testnet": true
  },
  
  "trading": {
    "max_position_size": 100000.0,
    "single_stock_max_ratio": 0.2,
    "max_positions": 10
  },
  
  "scanner": {
    "interval_minutes": 5,
    "min_price": 1.0,
    "max_price": 1000.0,
    "min_volume": 1000000,
    "min_turnover_rate": 0.01,
    "top_n": 10
  },
  
  "risk": {
    "stop_loss_ratio": 0.05,
    "take_profit_ratio": 0.15,
    "max_daily_loss": 0.03,
    "trailing_stop_ratio": 0.03,
    "max_drawdown": 0.1
  },
  
  "strategy": {
    "momentum": {
      "enabled": true,
      "rsi_period": 14,
      "rsi_oversold": 30,
      "rsi_overbought": 70,
      "ma_period": 20,
      "volume_factor": 1.5
    }
  },
  
  "logging": {
    "level": "INFO",
    "console": true,
    "file": true,
    "file_path": "logs/trading.log"
  }
}
```

## 配置项详解

### 1. Exchange（交易所配置）

```json
{
  "exchange": {
    "type": "FUTU",           // 交易所类型: FUTU, IBKR, BINANCE
    "is_simulation": true     // true=模拟盘, false=实盘
  }
}
```

### 2. Futu（富途配置）

```json
{
  "futu": {
    "host": "127.0.0.1",      // OpenD服务地址
    "port": 11111,            // OpenD端口
    "unlock_password": "",    // 解锁密码（实盘需要）
    "market": "HK"            // 市场代码: HK, US, CN
  }
}
```

### 3. IBKR（盈透配置）

```json
{
  "ibkr": {
    "host": "127.0.0.1",      // TWS/Gateway地址
    "port": 7496,             // TWS端口（7496=实盘, 7497=模拟盘）
    "client_id": 0,           // 客户端ID
    "account": ""             // 账户ID
  }
}
```

### 4. Binance（币安配置）

```json
{
  "binance": {
    "api_key": "",            // API密钥
    "api_secret": "",         // API密钥对应的secret
    "testnet": true           // 是否使用测试网
  }
}
```

### 5. Trading（交易参数）

```json
{
  "trading": {
    "max_position_size": 100000.0,      // 最大持仓金额（美元）
    "single_stock_max_ratio": 0.2,      // 单只股票最大占总资金比例
    "max_positions": 10                 // 最多同时持仓数量
  }
}
```

### 6. Scanner（扫描参数）

```json
{
  "scanner": {
    "interval_minutes": 5,              // 扫描间隔（分钟）
    "min_price": 1.0,                   // 最低股价筛选
    "max_price": 1000.0,                // 最高股价筛选
    "min_volume": 1000000,              // 最小成交量
    "min_turnover_rate": 0.01,          // 最小换手率
    "top_n": 10                         // 选出前N只股票
  }
}
```

### 7. Risk（风险管理）

```json
{
  "risk": {
    "stop_loss_ratio": 0.05,            // 止损比例（5%）
    "take_profit_ratio": 0.15,          // 止盈比例（15%）
    "max_daily_loss": 0.03,             // 每日最大亏损（3%）
    "trailing_stop_ratio": 0.03,        // 移动止损比例
    "max_drawdown": 0.1                 // 最大回撤（10%）
  }
}
```

### 8. Strategy（策略参数）

```json
{
  "strategy": {
    "momentum": {
      "enabled": true,                  // 是否启用动量策略
      "rsi_period": 14,                 // RSI周期
      "rsi_oversold": 30,               // RSI超卖线
      "rsi_overbought": 70,             // RSI超买线
      "ma_period": 20,                  // 移动平均线周期
      "volume_factor": 1.5              // 成交量倍数因子
    }
  }
}
```

### 9. Logging（日志配置）

```json
{
  "logging": {
    "level": "INFO",                    // 日志级别: DEBUG, INFO, WARNING, ERROR
    "console": true,                    // 是否输出到控制台
    "file": true,                       // 是否输出到文件
    "file_path": "logs/trading.log"     // 日志文件路径
  }
}
```

## 使用方法

### 启动时指定配置文件

```bash
# 使用默认config.json
./build/quant-trading-system

# 指定配置文件
./build/quant-trading-system config.json
./build/quant-trading-system my_config.json

# 使用旧的文本格式（向后兼容）
./build/quant-trading-system config.json
```

### 在代码中访问配置

```cpp
#include "config/config_manager.h"

auto& config_mgr = ConfigManager::getInstance();
config_mgr.loadFromFile("config.json");

const auto& config = config_mgr.getConfig();

// 访问交易所配置
std::string exchange_type = config.exchange.type;
bool is_simulation = config.exchange.is_simulation;

// 访问Futu配置
std::string host = config.futu.host;
int port = config.futu.port;

// 访问交易参数
double max_position = config.trading.max_position_size;
int max_positions = config.trading.max_positions;

// 访问风险参数
double stop_loss = config.risk.stop_loss_ratio;
double take_profit = config.risk.take_profit_ratio;

// 访问策略参数
if (config.strategy.momentum.enabled) {
    int rsi_period = config.strategy.momentum.rsi_period;
    int ma_period = config.strategy.momentum.ma_period;
}
```

## 配置格式说明

系统仅支持 JSON 格式的配置文件。请使用 `config.json`（或任意以 `.json` 结尾的文件名）来提供运行时配置，确保字段与文档中示例一致。

## 配置验证

### 必需字段

以下字段必须配置：
- `exchange.type` - 交易所类型
- `exchange.is_simulation` - 交易模式

### 合理范围

建议配置范围：
- `trading.single_stock_max_ratio`: 0.1 ~ 0.3
- `trading.max_positions`: 5 ~ 20
- `risk.stop_loss_ratio`: 0.03 ~ 0.10
- `risk.take_profit_ratio`: 0.10 ~ 0.30
- `risk.max_daily_loss`: 0.02 ~ 0.05

## 多环境配置

可以为不同环境创建不同的配置文件：

```bash
# 开发环境
config.dev.json

# 模拟盘
config.sim.json

# 实盘
config.prod.json
```

启动时指定：
```bash
./build/quant-trading-system config.prod.json
```

## JSON解析器

系统内置了轻量级的JSON解析器，无需外部依赖：

### 特性
- ✅ 完整的JSON语法支持
- ✅ 对象、数组、字符串、数字、布尔值
- ✅ 嵌套结构
- ✅ 转义字符支持
- ✅ 类型安全的值访问
- ✅ 默认值支持

### API示例

```cpp
#include "utils/json_parser.h"

// 解析JSON字符串
JsonValue json = JsonParser::parse(json_string);

// 解析JSON文件
JsonValue json = JsonParser::parseFile("config.json");

// 访问值
std::string type = json["exchange"]["type"].asString("FUTU");
int port = json["futu"]["port"].asInt(11111);
bool enabled = json["strategy"]["momentum"]["enabled"].asBool(true);
double ratio = json["risk"]["stop_loss_ratio"].asDouble(0.05);

// 检查键是否存在
if (json.hasKey("exchange")) {
    // ...
}

// 遍历对象
for (const auto& [key, value] : json.asObject()) {
    std::cout << key << std::endl;
}
```

## 最佳实践

1. **使用版本控制** - 将配置文件加入git，但敏感信息（密码、API密钥）使用环境变量
2. **注释说明** - 使用`_comment`字段添加说明
3. **合理分组** - 相关配置放在同一对象下
4. **默认值** - 为所有配置项提供合理的默认值
5. **环境隔离** - 开发、测试、生产使用不同的配置文件
6. **敏感信息** - API密钥、密码等不要直接写在配置文件中

## 迁移指南

如果你原来使用的是非 JSON 格式的配置文件，请将其转换为 JSON 并保存为 `config.json`。上文已给出 JSON 示例；确保字段名与示例一致（例如 `exchange.type`、`futu.host`、`futu.port` 等）。

## 故障排查

### 常见错误

1. **JSON格式错误**
   ```
   Failed to load JSON config: Invalid JSON value
   ```
   检查JSON语法，确保括号、引号、逗号正确

2. **文件不存在**
   ```
   Failed to open file: config.json
   ```
   确认文件路径正确，文件存在

3. **类型转换错误**
   ```
   Invalid value for port
   ```
   检查数值类型配置是否为数字而非字符串

### 验证JSON格式

使用在线工具验证JSON格式：
- https://jsonlint.com/
- https://jsonformatter.org/

或使用命令行工具：
```bash
# 使用Python验证
python -m json.tool config.json

# 使用jq验证
jq . config.json
```

## 总结

JSON配置系统提供了更强大、更灵活的配置管理方式，同时保持对旧格式的兼容性。建议新项目使用JSON格式，享受其带来的便利和可维护性优势。
