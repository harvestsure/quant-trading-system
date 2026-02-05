#pragma once

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

// K线周期枚举
enum class KlineInterval {
    K_1M, // minute
    K_3M,
    K_5M,
    K_15M,
    K_30M,
    K_1H, // hour
    K_4H,
    K_1D, // day
    K_1W,  // week
    K_1MO, // month
    K_1Y  // year
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
