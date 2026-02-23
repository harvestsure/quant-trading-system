# Quantitative Trading System

A C++ based multi-exchange quantitative trading system supporting Futu, IBKR, Binance and other trading platforms with both live and paper trading modes, implementing momentum chasing strategies.

## Quick Start

### 1. Quick Build (Recommended for Beginners)

The simplest build method using default configuration:

```bash
./build.sh
```

This will build a Release version with Futu exchange compiled from source (automatically adapts to ARM64 or x86_64).

### 2. Development Mode (Debug Build)

```bash
./build.sh --debug
```

### 3. Using Prebuilt Libraries (Faster Compilation)

**Using ARM64 Prebuilt Libraries (Recommended for Apple Silicon Mac):**

```bash
# Step 1: Precompile ARM64 libraries (only need to run once)
./build_ftapi_arm64.sh both

# Step 2: Build the project
./build.sh --use-prebuilt-arm64 --release
```

**Using x86_64 Prebuilt Libraries (Compatible with all Macs):**

```bash
./build.sh --use-prebuilt-x86
```

### 4. Run the System

```bash
./build/quant-trading-system config.json
```

## Build Options

### Basic Options
```bash
./build.sh [options]

Exchange Options:
  --enable-futu              Enable Futu exchange (default)
  --disable-futu             Disable Futu exchange
  --enable-ibkr              Enable IBKR exchange
  --enable-binance           Enable Binance exchange

Build Type:
  --debug                    Debug build (with debug symbols)
  --release                  Release build (optimized version, default)

FTAPI Build Mode (Futu only):
  --from-source              Build from source (default, native performance)
  --use-prebuilt-x86         Use x86_64 prebuilt libraries
  --use-prebuilt-arm64       Use ARM64 prebuilt libraries

Other:
  --ftapi-home <path>        Specify FTAPI4CPP SDK path
  --help                     Show help information
```

### Common Build Command Examples

```bash
# Default build (Release, from source)
./build.sh

# Debug build
./build.sh --debug

# Using ARM64 prebuilt libraries (fastest)
./build_ftapi_arm64.sh both  # only need to run once
./build.sh --use-prebuilt-arm64 --release

# Specify FTAPI path
./build.sh --ftapi-home /path/to/FTAPI4CPP_<version>

# Enable multiple exchanges
./build.sh --enable-ibkr --debug

# Using environment variables
export FTAPI_HOME=/path/FTAPI4CPP_<version>
./build.sh --use-prebuilt-arm64
```

## Build Mode Comparison

| Mode | Command | Build Time | Performance | Use Case |
|------|---------|------------|-------------|----------|
| From Source | `./build.sh` | 2-3 minutes | ⭐⭐⭐⭐⭐ | Development, debugging |
| ARM64 Prebuilt | `./build.sh --use-prebuilt-arm64` | 30 seconds | ⭐⭐⭐⭐⭐ | Production deployment (Apple Silicon) |
| x86_64 Prebuilt | `./build.sh --use-prebuilt-x86` | 30 seconds | ⭐⭐⭐⭐ | Quick testing, Intel Mac |

## Features

- ✅ **Multi-Exchange Support**: Support for Futu, IBKR, Binance and other trading platforms
- ✅ **Dual-Mode Trading**: Support for both live and paper trading
- ✅ **Market Scanning**: Periodic market scans to identify trading opportunities
- ✅ **Strategy System**: Flexible strategy manager supporting multiple concurrent strategies
- ✅ **Configuration Management**: JSON configuration files supporting multiple exchange configs
- ✅ **Data Subscription**: Real-time subscription to K-line and ticker data
- ✅ **Position Management**: Automatic tracking and management of all positions
- ✅ **Risk Control**: Comprehensive risk management system including stop-loss, take-profit, trailing stop-loss, and position sizing

## Documentation

- 📖 [BUILD_GUIDE.md](BUILD_GUIDE.md) - Complete build guide (detailed explanation of all build options)
- 📖 [docs/BUILD_OPTIONS.md](docs/BUILD_OPTIONS.md) - Build options quick reference
- 📖 [docs/DYNAMIC_STRATEGY_MANAGEMENT.md](docs/DYNAMIC_STRATEGY_MANAGEMENT.md) - Dynamic strategy management
- 📖 [docs/EVENT_DRIVEN_ARCHITECTURE.md](docs/EVENT_DRIVEN_ARCHITECTURE.md) - Event-driven architecture
- 📖 [docs/EXCHANGE_ABSTRACTION.md](docs/EXCHANGE_ABSTRACTION.md) - Exchange abstraction layer
- 📖 [docs/EXCHANGE_BUILD_OPTIONS.md](docs/EXCHANGE_BUILD_OPTIONS.md) - Exchange build options
- 📖 [docs/JSON_CONFIGURATION.md](docs/JSON_CONFIGURATION.md) - JSON configuration guide

## FAQ

### Q: Cannot find FTAPI SDK
```bash
# Method 1: Use parameter
./build.sh --ftapi-home /path/FTAPI4CPP_<version>

# Method 2: Use environment variable
export FTAPI_HOME=/path/FTAPI4CPP_<version>
./build.sh
```

## System Architecture

```
quant-trading-system/
├── include/               # Header files
│   ├── config/           # Configuration management
│   ├── managers/         # Core managers
│   ├── scanner/          # Market scanner
│   ├── data/             # Data subscription
│   ├── trading/          # Trade execution
│   ├── strategies/       # Trading strategies
│   └── utils/            # Utility classes
├── src/                  # Source files
├── config.json            # Configuration file
└── CMakeLists.txt        # Build configuration
```

### Core Modules

1. **ConfigManager**: Configuration manager, reads all configs from file
2. **PositionManager**: Position manager, tracks all position states
3. **RiskManager**: Risk manager, controls risk and position sizing
4. **StrategyManager**: Strategy manager, manages multiple strategy instances
5. **MarketScanner**: Market scanner, periodically scans market for opportunities
6. **DataSubscriber**: Data subscriber, subscribes to real-time market data
7. **OrderExecutor**: Order executor, handles order placement and management

## Build and Installation

### Prerequisites

- C++17 or higher
- CMake 3.15 or higher
- `nlohmann/json` (project uses git submodule stored in `libraries/json`, CMake will use this submodule first)
- Choose the corresponding API library based on the exchange used:
  - **Futu**: FTAPI4CPP (needs to be downloaded and configured separately)
  - **IBKR**: Interactive Brokers TWS API
  - **Binance**: Official Binance API (requires API Key and Secret)

### Get Dependencies (Git Submodule)

The project now uses `nlohmann/json` as a submodule (located in `libraries/json`). After first checkout, execute:

```bash
git submodule update --init --recursive
```

If you haven't added the submodule yet, run:

```bash
git submodule add https://github.com/nlohmann/json.git libraries/json
git submodule update --init --recursive
```

### Configure Futu API (FTAPI4CPP)

If Futu exchange support is needed, FTAPI4CPP library needs to be configured first.

#### Step 1: Get FTAPI4CPP

Download FTAPI4CPP from Futu's official website (e.g., version <version> or others), extract to a local directory. Directory structure should be:

```
FTAPI4CPP_<version>/
├── Include/          # Header files directory
│   ├── FTAPI.h
│   ├── FTSPI.h
│   ├── FTAPIChannel.h
│   └── ...
└── Bin/              # Library files directory
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

#### Step 2: Specify FTAPI_HOME During Build

There are **3 ways** to configure the FTAPI path:

**Method 1: Command line parameter (Recommended)**

```bash
chmod +x build.sh
./build.sh --ftapi-home /path/to/FTAPI4CPP_<version> --debug
```

**Method 2: Environment variable**

```bash
export FTAPI_HOME=/path/to/FTAPI4CPP_<version>
./build.sh --debug
```

**Method 3: Direct CMake call**

```bash
mkdir -p build
cd build
cmake -DFTAPI_HOME=/path/to/FTAPI4CPP_<version> -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)  # macOS: use make -j$(sysctl -n hw.ncpu)
```

### Build Steps

#### Using Build Script (Recommended)

```bash
# Enable Futu exchange, Release mode
./build.sh --ftapi-home /path/to/FTAPI4CPP_<version>

# Enable Futu exchange, Debug mode
./build.sh --ftapi-home /path/to/FTAPI4CPP_<version> --debug

# Disable Futu, enable IBKR
./build.sh --disable-futu --enable-ibkr

# View all available options
./build.sh --help
```

#### Manual Build

```bash
mkdir -p build
cd build
cmake -DFTAPI_HOME=/path/to/FTAPI4CPP_<version> -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)  # macOS
# or
make -j$(nproc)  # Linux
```

#### Build Script Parameter Explanation

```
Parameter Explanation:
  --enable-futu              Enable Futu exchange support (default: ON)
  --disable-futu             Disable Futu exchange support
  --enable-ibkr              Enable IBKR exchange support (default: OFF)
  --enable-binance           Enable Binance exchange support (default: OFF)
  --ftapi-home <path>        Specify FTAPI4CPP main directory
  --debug                    Build in Debug mode
  --release                  Build in Release mode (default)
  --help                     Show help information
```

## Configuration Guide

Edit the `config.json` file to configure system parameters. The system uses JSON format configuration supporting multiple exchanges:

### Basic Configuration

```json
{
  "exchange": {
    "type": "FUTU",           // Exchange type: FUTU, IBKR, BINANCE
    "is_simulation": true      // true=paper trading, false=live trading
  }
}
```

### Exchange connection configuration

**Futu configuration**:
```json
"futu": {
  "host": "127.0.0.1",        // OpenD server address
  "port": 11111,               // OpenD port
  "unlock_password": "",       // Unlock password (optional)
  "market": "HK"               // Market code: HK, US, etc.
}
```

**IBKR configuration**:
```json
"ibkr": {
  "host": "127.0.0.1",        // TWS server address
  "port": 7496,                // TWS port
  "client_id": 0,              // Client ID
  "account": ""                // Account ID
}
```

**Binance configuration**:
```json
"binance": {
  "api_key": "",              // API Key
  "api_secret": "",           // API Secret
  "testnet": true              // true=testnet, false=mainnet
}
```

### Position and Risk Management

```json
"trading": {
  "max_position_size": 100000.0,    // Maximum position size
  "single_stock_max_ratio": 0.2,    // Maximum ratio per stock (20%)
  "max_positions": 10               // Maximum number of concurrent positions
}
```

### Market Scanning Parameters

```json
"scanner": {
  "interval_minutes": 5,       // Scan interval (minutes)
  "min_price": 1.0,            // Minimum price
  "max_price": 1000.0,         // Maximum price
  "min_volume": 1000000,       // Minimum trading volume
  "min_turnover_rate": 0.01,   // Minimum turnover rate
  "top_n": 10                  // Return top N candidates
}
```

### Risk Management

```json
"risk": {
  "stop_loss_ratio": 0.05,      // Stop loss ratio (5%)
  "take_profit_ratio": 0.15,    // Take profit ratio (15%)
  "max_daily_loss": 0.03,       // Maximum daily loss (3%)
  "trailing_stop_ratio": 0.03,  // Trailing stop ratio (3%)
  "max_drawdown": 0.1           // Maximum drawdown limit (10%)
}
```

### Strategy Parameters

```json
"strategy": {
  "momentum": {
    "enabled": true,           // Enable momentum strategy
    "rsi_period": 14,          // RSI period
    "rsi_oversold": 30,        // RSI oversold threshold
    "rsi_overbought": 70,      // RSI overbought threshold
    "ma_period": 20,           // Moving average period
    "volume_factor": 1.5       // Volume factor
  }
}
```

### Logging Configuration

```json
"logging": {
  "level": "INFO",            // Log level: DEBUG, INFO, WARNING, ERROR
  "console": true,             // Output to console
  "file": true,                // Output to file
  "file_path": "logs/trading.log"  // Log file path
}
```

## Usage Guide

### 1. Start the System

```bash
./quant-trading-system config.json
```

### 2. System Workflow

1. **Startup**: System loads configuration and connects to selected exchange (Futu/IBKR/Binance)
2. **Scanning**: Periodically scans market and filters trading opportunities
3. **Analysis**: Strategy receives scan results and performs technical analysis
4. **Trading**: Automatically places orders when conditions are met
5. **Monitoring**: Real-time monitoring of positions, auto-closing triggered by stop-loss/take-profit/trailing stop-loss
6. **Reporting**: Periodically outputs system status and position information

### 3. Stop the System

Press `Ctrl+C` to gracefully exit the system.

## Strategy Description

### Momentum Chasing Strategy (MomentumStrategy)

**Core idea**: Track strongly rising stocks and buy after trend confirmation.

**Entry conditions**:
- Stock is in uptrend (price above 20-day MA)
- RSI between 30-70 (avoid overbought/oversold)
- Change between 2%-6%
- Turnover rate > 2% (sufficient liquidity)

**Exit conditions**:
- Trigger stop-loss (loss 5%)
- Trigger take-profit (profit 15%)
- Trend reversal

**Technical indicators**:
- RSI (Relative Strength Index)
- MA20 (20-day moving average)
- Volume analysis

## Developing Custom Strategies

Inherit from `StrategyBase` class to create your own strategy:

```cpp
#include "strategies/strategy_base.h"

class MyStrategy : public StrategyBase {
public:
    MyStrategy() : StrategyBase("MyStrategy") {}
    
    void onScanResult(const ScanResult& result) override {
        // Handle scan result
        if (/* your conditions */) {
            // Subscribe to data
            subscribeStock(result.symbol);
            
            // Place order
            buy(result.symbol, quantity, price);
        }
    }
    
    void onKLine(const std::string& symbol, const KLine& kline) override {
        // Handle K-line data
        // Implement your trading logic
    }
};
```

Add your strategy in `main.cpp`:

```cpp
auto my_strategy = std::make_shared<MyStrategy>();
strategy_mgr.addStrategy(my_strategy);
```

## Risk Control

The system includes multiple layers of risk control:

1. **Position limits**
  - Single stock max allocation: 20% of total capital
  - Maximum concurrent positions: 10
  - Total allocated capital should not exceed configured maximum

2. **Stop-loss / Take-profit**
  - Automatic stop-loss: 5% loss
  - Automatic take-profit: 15% profit

3. **Daily risk control**
  - Daily maximum loss limit: 3%
  - Trading halted for the day when triggered

4. **Order-level risk checks**
  - Verify sufficient funds before placing each order
  - Enforce position count limits
  - Enforce per-stock allocation limits

## Logging System

The system automatically logs all operations to `trading_system.log`:

```
2025-01-15 10:00:00.123 [INFO] System started
2025-01-15 10:05:00.456 [INFO] Market scan completed: found 5 stocks
2025-01-15 10:05:01.789 [INFO] Order placed: HK.00700 BUY 200 @ 350.0
2025-01-15 10:10:00.234 [WARNING] Stop loss triggered for HK.00700
```

## Important Notes

⚠️ **Important Notice**:

1. **Start with paper trading**: thoroughly test before using live trading
2. **Exchange configuration**: configure connection parameters and credentials per exchange
3. **API integration**: ensure the exchange API libraries are integrated correctly
4. **Capital safety**: set risk parameters reasonably and within your tolerance
5. **Monitoring**: monitor system status and logs during operation
6. **Permissions**: ensure API credentials have required trading permissions

## TODO Integration Checklist

Framework is implemented; the following locations need integration with real Futu API calls:

1. `src/scanner/market_scanner.cpp` - Market scanner API calls
2. `src/data/data_subscriber.cpp` - Data subscription API calls
3. `src/trading/order_executor.cpp` - Order placement API calls
4. `src/main.cpp` - API connection and initialization

## Performance Monitoring

The system outputs a status report every minute:

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

## License

This project is intended for learning and research purposes only.

## Contact

For issues or suggestions, please open an Issue.

---

**Risk Notice**: The stock market involves risks; invest cautiously. This system is for reference only; users assume responsibility for trading outcomes.
