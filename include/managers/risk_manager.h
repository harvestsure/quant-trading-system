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
    
    // 风险检查
    bool checkOrderRisk(const std::string& symbol, int quantity, double price);
    
    // 检查是否需要止损/止盈
    bool shouldStopLoss(const std::string& symbol, double current_price);
    bool shouldTakeProfit(const std::string& symbol, double current_price);
    
    // 计算建议仓位
    int calculatePositionSize(double stock_price, double available_cash);
    
    // 更新风险指标
    void updateDailyPnL(double pnl);
    void recordTrade(bool is_winning);
    
    // 获取风险指标
    RiskMetrics getRiskMetrics() const;
    
    // 重置每日统计
    void resetDailyMetrics();
    
    // 禁止拷贝
    RiskManager(const RiskManager&) = delete;
    RiskManager& operator=(const RiskManager&) = delete;
    
private:
    RiskManager();
    
    RiskMetrics metrics_;
    double initial_capital_;
    double current_capital_;
    mutable std::mutex mutex_;
};

 
