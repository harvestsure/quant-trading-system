#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>

// Use nlohmann::json from the third-party library
#include <nlohmann/json.hpp>

 

// Single exchange instance configuration
struct ExchangeInstanceConfig {
    std::string name;  // Exchange identifier: futu, ibkr, binance
    bool is_enabled = true;
    bool is_simulation = true;
    nlohmann::json params;  // Exchange-specific parameters; parsed by the exchange implementation
};

// Trading parameters
struct TradingParams {
    double max_position_size = 100000.0;
    double single_stock_max_ratio = 0.2;
    int max_positions = 10;
};

// Scanner parameters
struct ScannerParams {
    int interval_minutes = 5;
    double min_price = 1.0;
    double max_price = 1000.0;
    double min_volume = 1000000;
    double min_turnover_rate = 0.01;
    int top_n = 10;

    // === Breakout stock selection parameters ===
    double breakout_volume_ratio_min = 2.5;   // minimum volume ratio
    double breakout_change_ratio_min = 0.02;  // minimum price change
    double breakout_change_ratio_max = 0.10;  // maximum price change (avoid chasing limit-up)
    double breakout_amplitude_min = 0.02;     // minimum amplitude
    double breakout_score_weight_volume = 35.0;  // volume score weight
    double breakout_score_weight_change = 25.0;  // change score weight
    double breakout_score_weight_speed = 25.0;   // speed score weight
    double breakout_score_weight_turnover = 15.0; // turnover score weight
};

// Risk management parameters
struct RiskParams {
    double stop_loss_ratio = 0.05;
    double take_profit_ratio = 0.15;
    double max_daily_loss = 0.03;
    double trailing_stop_ratio = 0.03;
    double max_drawdown = 0.1;
};

// Strategy parameters
struct MomentumStrategyParams {
    bool enabled = true;
    int rsi_period = 14;
    int rsi_oversold = 30;
    int rsi_overbought = 70;
    int ma_period = 20;
    double volume_factor = 1.5;

    // === Hong Kong breakout chase/exit parameters ===
    double breakout_volume_ratio = 3.0;     // breakout volume ratio threshold (current/avg >= 3x)
    double breakout_change_min = 0.03;      // minimum change 3%
    double breakout_change_max = 0.08;      // maximum change 8% (avoid chasing too high)
    double breakout_amplitude_min = 0.03;   // minimum amplitude 3%
    double breakout_turnover_min = 0.03;    // minimum turnover 3%
    double chase_trailing_stop = 0.025;     // trailing stop when chasing 2.5%
    double chase_hard_stop_loss = 0.03;     // hard stop loss 3%
    double chase_take_profit = 0.08;        // take profit 8%
    double chase_rsi_max = 80.0;            // RSI overbought upper bound
    double chase_rsi_min = 40.0;            // RSI lower bound
    double momentum_exit_speed = -0.005;    // momentum reversal exit threshold (speed < -0.5%)
    int momentum_stale_minutes = 15;        // momentum stale timeout (minutes); exit if no new highs
    double price_vs_high_max = 0.03;        // when chasing, price must be within 3% of the high
};

struct StrategyParams {
    MomentumStrategyParams momentum;
};

// Logging configuration
struct LoggingConfig {
    std::string level = "INFO";
    bool console = true;
    bool file = true;
    std::string file_dir = "logs/";
};

// Telegram notification configuration
struct TelegramConfig {
    bool enabled = false;
    std::string bot_token;
    std::string chat_id;
    int api_timeout_seconds = 5;
    size_t max_queue_size = 1000;
    bool batch_send = false;
    int batch_size = 10;
    int batch_interval_ms = 1000;
};

// Notification configuration
struct NotificationConfig {
    TelegramConfig telegram;
};

// Complete configuration structure
struct TradingConfig {
    std::vector<ExchangeInstanceConfig> exchanges;  // list of multiple exchange configurations
    TradingParams trading;
    ScannerParams scanner;
    RiskParams risk;
    StrategyParams strategy;
    LoggingConfig logging;
    NotificationConfig notification;
};

class ConfigManager {
public:
    static ConfigManager& getInstance();
    
    // Supports JSON and legacy text formats
    bool loadFromFile(const std::string& config_file);
    bool loadFromJson(const std::string& json_file);
    bool loadFromText(const std::string& text_file);
    
    const TradingConfig& getConfig() const { return config_; }
    
    // Convenience accessors - scanner params
    const ScannerParams& getScannerParams() const { return config_.scanner; }
    
    // Convenience accessors - multi-exchange support
    const std::vector<ExchangeInstanceConfig>& getExchanges() const { return config_.exchanges; }
    std::vector<ExchangeInstanceConfig> getEnabledExchanges() const;
    const ExchangeInstanceConfig* getExchange(const std::string& name) const;
    bool isSimulation() const { return config_.exchanges.empty() ? true : config_.exchanges[0].is_simulation; }
    
    // Convenience accessors - notification configuration
    const NotificationConfig& getNotificationConfig() const { return config_.notification; }
    const TelegramConfig& getTelegramConfig() const { return config_.notification.telegram; }
    
    // Non-copyable
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
private:
    ConfigManager() = default;
    TradingConfig config_;
    
    void parseJsonConfig(const nlohmann::json& j);
    void parseConfigLine(const std::string& line);
};

