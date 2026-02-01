#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>

// Use nlohmann::json from the third-party library
#include <nlohmann/json.hpp>

 

// 交易所配置
struct ExchangeConfig {
    std::string type = "FUTU";
    bool is_simulation = true;
};

// Futu配置
struct FutuConnectionConfig {
    std::string host = "127.0.0.1";
    int port = 11111;
    std::string unlock_password = "";
    std::string market = "HK";
};

// IBKR配置
struct IBKRConnectionConfig {
    std::string host = "127.0.0.1";
    int port = 7496;
    int client_id = 0;
    std::string account = "";
};

// Binance配置
struct BinanceConnectionConfig {
    std::string api_key = "";
    std::string api_secret = "";
    bool testnet = true;
};

// 交易参数
struct TradingParams {
    double max_position_size = 100000.0;
    double single_stock_max_ratio = 0.2;
    int max_positions = 10;
};

// 扫描参数
struct ScannerParams {
    int interval_minutes = 5;
    double min_price = 1.0;
    double max_price = 1000.0;
    double min_volume = 1000000;
    double min_turnover_rate = 0.01;
    int top_n = 10;
};

// 风险管理参数
struct RiskParams {
    double stop_loss_ratio = 0.05;
    double take_profit_ratio = 0.15;
    double max_daily_loss = 0.03;
    double trailing_stop_ratio = 0.03;
    double max_drawdown = 0.1;
};

// 策略参数
struct MomentumStrategyParams {
    bool enabled = true;
    int rsi_period = 14;
    int rsi_oversold = 30;
    int rsi_overbought = 70;
    int ma_period = 20;
    double volume_factor = 1.5;
};

struct StrategyParams {
    MomentumStrategyParams momentum;
};

// 日志配置
struct LoggingConfig {
    std::string level = "INFO";
    bool console = true;
    bool file = true;
    std::string file_path = "logs/trading.log";
};

// 完整配置结构
struct TradingConfig {
    ExchangeConfig exchange;
    FutuConnectionConfig futu;
    IBKRConnectionConfig ibkr;
    BinanceConnectionConfig binance;
    TradingParams trading;
    ScannerParams scanner;
    RiskParams risk;
    StrategyParams strategy;
    LoggingConfig logging;
};

class ConfigManager {
public:
    static ConfigManager& getInstance();
    
    // 支持JSON和旧的文本格式
    bool loadFromFile(const std::string& config_file);
    bool loadFromJson(const std::string& json_file);
    bool loadFromText(const std::string& text_file);
    
    const TradingConfig& getConfig() const { return config_; }
    
    // 便捷访问方法
    std::string getExchangeType() const { return config_.exchange.type; }
    bool isSimulation() const { return config_.exchange.is_simulation; }
    
    // 禁止拷贝
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
private:
    ConfigManager() = default;
    TradingConfig config_;
    
    void parseJsonConfig(const nlohmann::json& j);
    void parseConfigLine(const std::string& line);
};

