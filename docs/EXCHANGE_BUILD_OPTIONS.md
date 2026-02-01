# 交易所编译选项说明

## 概述

本系统支持多个交易所，每个交易所都可以通过CMake编译选项独立启用或禁用。这样设计的优势：

1. **按需编译**：只编译需要的交易所，减少依赖和编译时间
2. **灵活部署**：不同环境可以部署不同的交易所组合
3. **避免依赖问题**：如果没有某个交易所的API库，可以关闭该交易所的编译

## 支持的交易所

| 交易所 | CMake选项 | 默认状态 | API库要求 |
|--------|-----------|----------|-----------|
| Futu (富途) | `ENABLE_FUTU` | ON | futuquant |
| IBKR (盈透) | `ENABLE_IBKR` | OFF | TwsApi |
| Binance (币安) | `ENABLE_BINANCE` | OFF | binance-cpprest |

## 编译方法

### 方法一：使用build.sh脚本（推荐）

```bash
# 只编译Futu交易所（默认）
./build.sh

# 编译Futu和IBKR
./build.sh --enable-ibkr

# 编译Futu和Binance
./build.sh --enable-binance

# 只编译IBKR（禁用Futu）
./build.sh --disable-futu --enable-ibkr

# 编译所有交易所
./build.sh --enable-ibkr --enable-binance

# 查看帮助
./build.sh --help
```

### 方法二：直接使用CMake

```bash
# 创建构建目录
mkdir build && cd build

# 只编译Futu
cmake ..

# 编译Futu和IBKR
cmake .. -DENABLE_FUTU=ON -DENABLE_IBKR=ON

# 只编译IBKR
cmake .. -DENABLE_FUTU=OFF -DENABLE_IBKR=ON

# 编译所有交易所
cmake .. -DENABLE_FUTU=ON -DENABLE_IBKR=ON -DENABLE_BINANCE=ON

# 编译
make -j$(nproc)
```

## 安装交易所API库

### Futu OpenD

```bash
# 1. 下载Futu OpenD
# 访问：https://openapi.futunn.com/futu-api-doc/
# 下载对应平台的OpenD客户端

# 2. 将API库放到项目目录
mkdir -p futu-api/include
mkdir -p futu-api/lib

# 3. 复制头文件和库文件
cp /path/to/futu/include/* futu-api/include/
cp /path/to/futu/lib/* futu-api/lib/

# 4. 启动OpenD
./FutuOpenD -cfg_file config.ini
```

### Interactive Brokers TWS API

```bash
# 1. 下载TWS API
# 访问：https://www.interactivebrokers.com/en/index.php?f=5041
# 下载TWS API for C++

# 2. 安装TWS API
# Linux: sudo dpkg -i twsapi_macunix.976.01.jar
# 或从源码编译

# 3. 将API库放到项目目录
mkdir -p ibkr-api/include
mkdir -p ibkr-api/lib

cp -r /opt/IBJts/source/cppclient/client/* ibkr-api/include/
cp /opt/IBJts/source/cppclient/client/*.so ibkr-api/lib/

# 4. 安装并运行TWS或IB Gateway
```

### Binance API

```bash
# 1. 安装依赖
sudo apt-get install libcpprest-dev libssl-dev

# 2. 克隆Binance C++ API
git clone https://github.com/binance/binance-connector-cpp.git

# 3. 编译安装
cd binance-connector-cpp
mkdir build && cd build
cmake ..
make
sudo make install

# 4. 链接到项目
mkdir -p binance-api/include
mkdir -p binance-api/lib
cp -r ../include/* binance-api/include/
cp libbinance-cpprest.* binance-api/lib/
```

## 代码中的条件编译

系统使用预处理器宏来控制交易所代码的编译：

### 在代码中检查交易所是否启用

```cpp
#ifdef ENABLE_FUTU
    // Futu交易所相关代码
    #include "exchange/futu_exchange.h"
#endif

#ifdef ENABLE_IBKR
    // IBKR交易所相关代码
    #include "exchange/ibkr_exchange.h"
#endif

#ifdef ENABLE_BINANCE
    // Binance交易所相关代码
    #include "exchange/binance_exchange.h"
#endif
```

### ExchangeFactory中的条件编译

```cpp
std::shared_ptr<IExchange> ExchangeFactory::createExchange(
    ExchangeType type,
    const std::map<std::string, std::string>& config) {
    
    switch (type) {
        case ExchangeType::FUTU:
#ifdef ENABLE_FUTU
            // 创建Futu交易所实例
            return std::make_shared<FutuExchange>(futu_config);
#else
            LOG_ERROR("Futu exchange is not enabled. Rebuild with -DENABLE_FUTU=ON");
            return nullptr;
#endif
        
        case ExchangeType::IBKR:
#ifdef ENABLE_IBKR
            // 创建IBKR交易所实例
            return std::make_shared<IBKRExchange>(ibkr_config);
#else
            LOG_ERROR("IBKR exchange is not enabled. Rebuild with -DENABLE_IBKR=ON");
            return nullptr;
#endif
        
        // ...
    }
}
```

## 运行时配置

在 `config.json` 中设置要使用的交易所：

```json
{
  "exchange": {
    "type": "FUTU"
  }
}
```

**注意**：运行时配置的交易所必须在编译时已经启用，否则会报错（请在 `config.json` 中确认 `exchange.type`）。

## 常见问题

### Q1: 编译时报错找不到交易所API头文件

**A**: 确保对应的API库已经安装到正确的目录：
- Futu: `futu-api/include/` 和 `futu-api/lib/`
- IBKR: `ibkr-api/include/` 和 `ibkr-api/lib/`
- Binance: `binance-api/include/` 和 `binance-api/lib/`

### Q2: 链接时报错找不到库文件

**A**: 检查以下几点：
1. 库文件是否在正确的目录
2. CMakeLists.txt中的`link_directories`是否正确
3. 库文件名称是否与`target_link_libraries`中的名称匹配

### Q3: 运行时提示"exchange is not enabled"

**A**: 这是因为配置文件中配置的交易所在编译时没有启用（请检查 `config.json` 中的 `exchange.type`）。需要重新编译：

```bash
# 如果要使用IBKR，需要启用IBKR编译
./build.sh --enable-ibkr
```

### Q4: 如何添加新的交易所

参考 [EXCHANGE_ABSTRACTION.md](./EXCHANGE_ABSTRACTION.md) 文档，步骤如下：

1. 创建交易所实现类（继承`IExchange`）
2. 在CMakeLists.txt中添加新的`option`
3. 在ExchangeFactory中添加条件编译代码
4. 更新 `config.json` 添加新的交易所类型

## 推荐配置
在 `config.json` 中设置要使用的交易所，例如：
### 开发环境

```bash
# 启用所有交易所，方便开发和测试
./build.sh --enable-ibkr --enable-binance
```

### 生产环境（只用Futu）

```bash
# 只编译需要的交易所，减少依赖
./build.sh
```

### 生产环境（多交易所）

```bash
# 根据实际使用的交易所启用
./build.sh --enable-ibkr
```

## 编译输出示例

```
======================================
  Building Futu Quant Trading System
======================================

Exchange Configuration:
  Futu:    ON
  IBKR:    ON
  Binance: OFF

Running CMake...
-- Futu Exchange: ENABLED
-- IBKR Exchange: ENABLED
-- Configuring done
-- Generating done
-- Build files have been written to: /path/to/build

Compiling...
[ 10%] Building CXX object CMakeFiles/quant-trading-system.dir/src/exchange/futu_exchange.cpp.o
[ 20%] Building CXX object CMakeFiles/quant-trading-system.dir/src/exchange/ibkr_exchange.cpp.o
...
[100%] Linking CXX executable quant-trading-system

======================================
  Build completed successfully!
======================================
```

## 总结

通过CMake的编译选项机制，系统可以灵活地控制哪些交易所要编译进最终的可执行文件。这种设计使得系统可以适应不同的部署环境和需求，同时避免不必要的依赖问题。
