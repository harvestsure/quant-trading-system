#include "managers/risk_manager.h"
#include "managers/position_manager.h"
#include "config/config_manager.h"
#include "utils/logger.h"
#include <cmath>
#include <sstream>

RiskManager& RiskManager::getInstance() {
    static RiskManager instance;
    return instance;
}

RiskManager::RiskManager() {
    const auto& config = ConfigManager::getInstance().getConfig();
    initial_capital_ = config.trading.max_position_size;
    current_capital_ = initial_capital_;
}

bool RiskManager::checkOrderRisk(const std::string& symbol, int quantity, double price) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const auto& config = ConfigManager::getInstance().getConfig();
    auto& pos_mgr = PositionManager::getInstance();
    
    // Check daily loss limit
    if (metrics_.daily_pnl_ratio < -config.risk.max_daily_loss) {
        LOG_WARN("Daily loss limit reached, order rejected");
        return false;
    }
    
    // Check position count
    if (!pos_mgr.hasPosition(symbol) && 
        pos_mgr.getTotalPositions() >= config.trading.max_positions) {
        LOG_WARN("Max positions limit reached, order rejected");
        return false;
    }
    
    // Check single stock ratio
    double order_value = std::abs(quantity * price);
    double total_value = pos_mgr.getTotalMarketValue() + order_value;
    double ratio = order_value / total_value;
    
    if (ratio > config.trading.single_stock_max_ratio) {
        std::stringstream ss;
        ss << "Single stock ratio " << ratio << " exceeds limit " 
           << config.trading.single_stock_max_ratio << ", order rejected";
        LOG_WARN(ss.str());
        return false;
    }
    
    // Check available capital
    if (order_value > current_capital_ * 0.95) {
        LOG_WARN("Insufficient capital, order rejected");
        return false;
    }
    
    return true;
}

bool RiskManager::shouldStopLoss(const std::string& symbol, double current_price) {
    const auto& config = ConfigManager::getInstance().getConfig();
    auto& pos_mgr = PositionManager::getInstance();
    
    Position* pos = pos_mgr.getPosition(symbol);
    if (!pos) return false;
    
    double loss_ratio = (current_price - pos->avg_price) / pos->avg_price;
    
    if (loss_ratio <= -config.risk.stop_loss_ratio) {
        std::stringstream ss;
        ss << "Stop loss triggered for " << symbol 
           << " loss_ratio=" << loss_ratio;
        LOG_WARN(ss.str());
        return true;
    }
    
    return false;
}

bool RiskManager::shouldTakeProfit(const std::string& symbol, double current_price) {
    const auto& config = ConfigManager::getInstance().getConfig();
    auto& pos_mgr = PositionManager::getInstance();
    
    Position* pos = pos_mgr.getPosition(symbol);
    if (!pos) return false;
    
    double profit_ratio = (current_price - pos->avg_price) / pos->avg_price;
    
    if (profit_ratio >= config.risk.take_profit_ratio) {
        std::stringstream ss;
        ss << "Take profit triggered for " << symbol 
           << " profit_ratio=" << profit_ratio;
        LOG_INFO(ss.str());
        return true;
    }
    
    return false;
}

int RiskManager::calculatePositionSize(double stock_price, double available_cash) {
    const auto& config = ConfigManager::getInstance().getConfig();
    auto& pos_mgr = PositionManager::getInstance();
    
    // Calculate max amount for single stock
    double max_stock_value = config.trading.max_position_size * config.trading.single_stock_max_ratio;
    
    // Consider current total positions
    double current_total = pos_mgr.getTotalMarketValue();
    double remaining = config.trading.max_position_size - current_total;
    
    double max_value = std::min({max_stock_value, available_cash * 0.95, remaining});
    
    if (max_value <= 0 || stock_price <= 0) return 0;
    
    // Calculate shares (full lots, minimum 100 shares for HK stocks)
    int shares = static_cast<int>(max_value / stock_price);
    shares = (shares / 100) * 100;  // Round down to multiple of 100
    
    return shares;
}

void RiskManager::updateDailyPnL(double pnl) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    metrics_.daily_pnl = pnl;
    if (initial_capital_ > 0) {
        metrics_.daily_pnl_ratio = pnl / initial_capital_;
    }
    
    current_capital_ = initial_capital_ + pnl;
}

void RiskManager::recordTrade(bool is_winning) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    metrics_.total_trades++;
    if (is_winning) {
        metrics_.winning_trades++;
    } else {
        metrics_.losing_trades++;
    }
}

RiskMetrics RiskManager::getRiskMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metrics_;
}

void RiskManager::resetDailyMetrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    metrics_.daily_pnl = 0.0;
    metrics_.daily_pnl_ratio = 0.0;
    

    LOG_INFO("Daily risk metrics reset");
}

