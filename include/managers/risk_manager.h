#pragma once

#include <string>
#include <map>
#include <mutex>

 

struct RiskMetrics {
    double daily_pnl = 0.0;
    double daily_pnl_ratio = 0.0;
    double max_drawdown = 0.0;
    double total_commission = 0.0;
    int total_trades = 0;
    int winning_trades = 0;
    int losing_trades = 0;
};

class RiskManager {
public:
    static RiskManager& getInstance();
    
    // Risk checks
    bool checkOrderRisk(const std::string& symbol, int quantity, double price);

    // Check stop-loss / take-profit conditions
    bool shouldStopLoss(const std::string& symbol, double current_price);
    bool shouldTakeProfit(const std::string& symbol, double current_price);

    // Calculate suggested position size
    int calculatePositionSize(double stock_price, double available_cash);

    // Update risk metrics
    void updateDailyPnL(double pnl);
    void recordTrade(bool is_winning);

    // Get risk metrics
    RiskMetrics getRiskMetrics() const;

    // Reset daily statistics
    void resetDailyMetrics();

    // Non-copyable
    RiskManager(const RiskManager&) = delete;
    RiskManager& operator=(const RiskManager&) = delete;
    
private:
    RiskManager();
    
    RiskMetrics metrics_;
    double initial_capital_;
    double current_capital_;
    mutable std::mutex mutex_;
};

 
