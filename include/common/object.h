#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "constant.h"
#include "utils/logger_defines.h"


// Tick data (unified format)
struct TickData {
    std::string symbol;
    std::string exchange;
    int64_t timestamp;
    std::string datetime;

    double last_price = 0.0;
    double open_price = 0.0;
    double high_price = 0.0;
    double low_price = 0.0;
    double pre_close = 0.0;

    int64_t volume = 0;
    double turnover = 0.0;
    double turnover_rate = 0.0;

    double bid_price_1 = 0.0;
    int64_t bid_volume_1 = 0;
    double ask_price_1 = 0.0;
    int64_t ask_volume_1 = 0;

    std::vector<double> bid_prices;
    std::vector<int64_t> bid_volumes;
    std::vector<double> ask_prices;
    std::vector<int64_t> ask_volumes;
};

// Kline data (unified format)
struct KlineData {
    std::string symbol;
    std::string exchange;
    int64_t timestamp;
    std::string datetime;
    std::string interval;

    // Corresponding enum interval; keep the string field for external config compatibility
    KlineInterval interval_enum = KlineInterval::K_1M;

    double open_price = 0.0;
    double high_price = 0.0;
    double low_price = 0.0;
    double close_price = 0.0;
    int64_t volume = 0;
    double turnover = 0.0;
};

// Order data (unified format)
struct OrderData {
    std::string order_id;
    std::string exchange_order_id;
    std::string symbol;
    std::string exchange;

    Direction direction = Direction::LONG;
    OrderType type = OrderType::LIMIT;
    OrderStatus status = OrderStatus::SUBMITTING;

    double price = 0.0;
    int64_t volume = 0;
    int64_t traded_volume = 0;

    int64_t create_time = 0;
    int64_t update_time = 0;

    std::string strategy_name;
    std::string error_msg;
};

// Trade data (unified format)
struct TradeData {
    std::string trade_id;
    std::string order_id;
    std::string exchange_order_id;
    std::string symbol;
    std::string exchange;

    Direction direction = Direction::LONG;

    double price = 0.0;
    int64_t volume = 0;
    int64_t timestamp = 0;

    std::string strategy_name;
};

// Position data (unified format)
struct PositionData {
    std::string symbol;
    std::string exchange;

    Direction direction = Direction::LONG;

    int64_t volume = 0;
    int64_t frozen_volume = 0;
    int64_t available_volume = 0;

    double avg_price = 0.0;
    double current_price = 0.0;
    double market_value = 0.0;
    double profit_loss = 0.0;
    double profit_loss_ratio = 0.0;
};

// Account data (unified format)
struct AccountData {
    std::string account_id;
    std::string exchange;

    double balance = 0.0;
    double available = 0.0;
    double frozen = 0.0;
    double market_value = 0.0;

    double profit_loss = 0.0;
    double profit_loss_ratio = 0.0;
};

// Trading signal data
struct SignalData {
    std::string symbol;
    std::string strategy_name;

    Direction direction = Direction::LONG;
    double price = 0.0;
    int64_t volume = 0;

    std::string reason;
    int64_t timestamp = 0;
};

// Log data
struct LogData {
    LogLevel level = LogLevel::Info;
    std::string message;
    int64_t timestamp = 0;
};
// Market snapshot (used for market scanning)
struct Snapshot {
    std::string symbol;           // Stock symbol
    std::string name;             // Stock name
    std::string exchange;         // Exchange
    int64_t timestamp;            // Timestamp (milliseconds)
    std::string datetime;         // Datetime string

    double last_price = 0.0;      // Latest price
    double open_price = 0.0;      // Open price
    double high_price = 0.0;      // High price
    double low_price = 0.0;       // Low price
    double pre_close = 0.0;       // Previous close price

    int64_t volume = 0;           // Volume
    double turnover = 0.0;        // Turnover
    double turnover_rate = 0.0;   // Turnover rate (%)

    double price_change = 0.0;    // Price change (%)
    double price_change_abs = 0.0;// Price change (absolute)

    double bid_price_1 = 0.0;     // Best bid price
    int64_t bid_volume_1 = 0;     // Best bid volume
    double ask_price_1 = 0.0;     // Best ask price
    int64_t ask_volume_1 = 0;     // Best ask volume
};