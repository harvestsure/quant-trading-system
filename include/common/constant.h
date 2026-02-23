#pragma once

// Order direction
enum class Direction {
    LONG = 0,
    SHORT = 1
};

// Order side (keeps compatibility with different naming conventions)
enum class OrderSide {
    BUY = 0,
    SELL = 1
};

// Order type
enum class OrderType {
    LIMIT = 0,
    MARKET = 1
};

// K-line (candlestick) interval enum
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

// Order status
enum class OrderStatus {
    SUBMITTING = 0,
    SUBMITTED = 1,
    PARTIAL_FILLED = 2,
    FILLED = 3,
    CANCELLED = 4,
    REJECTED = 5,
    FAILED = 6
};
