#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>

// Use nlohmann::json from the third-party library
#include <nlohmann/json.hpp>

 

// 单个交易所配置
struct ExchangeInstanceConfig {
    std::string name;  // 交易所标识: futu, ibkr, binance
    bool is_enabled = true;
    bool is_simulation = true;
    nlohmann::json params;  // 交易所特定参数，由交易所自己解析处理
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

    // === 爆发股筛选参数 ===
    double breakout_volume_ratio_min = 2.5;   // 最小量比
    double breakout_change_ratio_min = 0.02;  // 最小涨幅
    double breakout_change_ratio_max = 0.10;  // 最大涨幅（避免追涨停）
    double breakout_amplitude_min = 0.02;     // 最小振幅
    double breakout_score_weight_volume = 35.0;  // 量比评分权重
    double breakout_score_weight_change = 25.0;  // 涨幅评分权重
    double breakout_score_weight_speed = 25.0;   // 涨速评分权重
    double breakout_score_weight_turnover = 15.0; // 换手率评分权重
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

    // === 港股爆发追涨杀跌参数 ===
    double breakout_volume_ratio = 3.0;     // 爆发量比阈值（当前量/均量 >= 3倍）
    double breakout_change_min = 0.03;      // 最小涨幅 3%
    double breakout_change_max = 0.08;      // 最大涨幅 8%（避免追太高）
    double breakout_amplitude_min = 0.03;   // 最小振幅 3%
    double breakout_turnover_min = 0.03;    // 最小换手率 3%
    double chase_trailing_stop = 0.025;     // 追涨移动止损 2.5%
    double chase_hard_stop_loss = 0.03;     // 硬止损 3%
    double chase_take_profit = 0.08;        // 止盈 8%
    double chase_rsi_max = 80.0;            // RSI 超买上限
    double chase_rsi_min = 40.0;            // RSI 下限
    double momentum_exit_speed = -0.005;    // 动量反转退出阈值（涨速低于-0.5%时退出）
    int momentum_stale_minutes = 15;        // 动量停滞超时（分钟），超时无新高则退出
    double price_vs_high_max = 0.03;        // 追涨时距最高价不超过3%
};

struct StrategyParams {
    MomentumStrategyParams momentum;
};

// 日志配置
struct LoggingConfig {
    std::string level = "INFO";
    bool console = true;
    bool file = true;
    std::string file_dir = "logs/";
};

// Telegram通知配置
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

// 通知配置
struct NotificationConfig {
    TelegramConfig telegram;
};

// 完整配置结构
struct TradingConfig {
    std::vector<ExchangeInstanceConfig> exchanges;  // 多交易所配置列表
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
    
    // 支持JSON和旧的文本格式
    bool loadFromFile(const std::string& config_file);
    bool loadFromJson(const std::string& json_file);
    bool loadFromText(const std::string& text_file);
    
    const TradingConfig& getConfig() const { return config_; }
    
    // 便捷访问方法 - 扫描器参数
    const ScannerParams& getScannerParams() const { return config_.scanner; }
    
    // 便捷访问方法 - 多交易所支持
    const std::vector<ExchangeInstanceConfig>& getExchanges() const { return config_.exchanges; }
    std::vector<ExchangeInstanceConfig> getEnabledExchanges() const;
    const ExchangeInstanceConfig* getExchange(const std::string& name) const;
    bool isSimulation() const { return config_.exchanges.empty() ? true : config_.exchanges[0].is_simulation; }
    
    // 便捷访问方法 - 通知配置
    const NotificationConfig& getNotificationConfig() const { return config_.notification; }
    const TelegramConfig& getTelegramConfig() const { return config_.notification.telegram; }
    
    // 禁止拷贝
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
private:
    ConfigManager() = default;
    TradingConfig config_;
    
    void parseJsonConfig(const nlohmann::json& j);
    void parseConfigLine(const std::string& line);
};

