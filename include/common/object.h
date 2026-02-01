#pragma once

#include <string>
#include <vector>
#include <cstdint>

// 基础数据结构与枚举，供全局使用

// 订单方向
enum class Direction {
    LONG = 0,
    SHORT = 1
};

// 订单买卖方向（兼容不同命名习惯）
enum class OrderSide {
    BUY = 0,
    SELL = 1
};

// 订单类型
enum class OrderType {
    LIMIT = 0,
    MARKET = 1
};

// 订单状态
enum class OrderStatus {
    SUBMITTING = 0,
    SUBMITTED = 1,
    PARTIAL_FILLED = 2,
    FILLED = 3,
    CANCELLED = 4,
    REJECTED = 5,
    FAILED = 6
};

// Tick数据（统一格式）
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

// K线数据（统一格式）
struct KlineData {
    std::string symbol;
    std::string exchange;
    int64_t timestamp;
    std::string datetime;
    std::string interval;

    double open_price = 0.0;
    double high_price = 0.0;
    double low_price = 0.0;
    double close_price = 0.0;
    int64_t volume = 0;
    double turnover = 0.0;
};

// 订单数据（统一格式）
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

// 成交数据（统一格式）
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

// 持仓数据（统一格式）
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

// 账户数据（统一格式）
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

// 交易信号数据
struct SignalData {
    std::string symbol;
    std::string strategy_name;

    Direction direction = Direction::LONG;
    double price = 0.0;
    int64_t volume = 0;

    std::string reason;
    int64_t timestamp = 0;
};

// 日志数据
struct LogData {
    std::string level;
    std::string message;
    int64_t timestamp = 0;
};
// 市场快照数据（用于市场扫描）
struct Snapshot {
    std::string symbol;           // 股票代码
    std::string exchange;         // 交易所
    int64_t timestamp;            // 时间戳（毫秒）
    std::string datetime;         // 时间字符串

    double last_price = 0.0;      // 最新价格
    double open_price = 0.0;      // 开盘价
    double high_price = 0.0;      // 最高价
    double low_price = 0.0;       // 最低价
    double pre_close = 0.0;       // 前收盘价

    int64_t volume = 0;           // 成交量
    double turnover = 0.0;        // 成交额
    double turnover_rate = 0.0;   // 换手率（%）

    double price_change = 0.0;    // 涨幅（%）
    double price_change_abs = 0.0;// 涨幅（绝对值）

    double bid_price_1 = 0.0;     // 买一价
    int64_t bid_volume_1 = 0;     // 买一量
    double ask_price_1 = 0.0;     // 卖一价
    int64_t ask_volume_1 = 0;     // 卖一量
};