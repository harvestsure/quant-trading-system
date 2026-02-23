#include "managers/position_manager.h"
#include "utils/logger.h"
#include <sstream>

PositionManager& PositionManager::getInstance() {
    static PositionManager instance;
    return instance;
}

void PositionManager::updatePosition(const std::string& symbol, int quantity, double price) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = positions_.find(symbol);
    if (it == positions_.end()) {
        // New position
        Position pos;
        pos.symbol = symbol;
        pos.quantity = quantity;
        pos.avg_price = price;
        pos.current_price = price;
        pos.cost = quantity * price;
        pos.market_value = pos.cost;
        pos.side = quantity > 0 ? "LONG" : "SHORT";
        calculateProfitLoss(pos);
        
        positions_[symbol] = pos;
        
        std::stringstream ss;
        ss << "New position opened: " << symbol << " qty=" << quantity 
           << " price=" << price;
        LOG_INFO(ss.str());
    } else {
        // Update position
        Position& pos = it->second;
        
        // Calculate new average cost
        double total_cost = pos.avg_price * pos.quantity + price * quantity;
        pos.quantity += quantity;
        
        if (pos.quantity == 0) {
            // Close position
            std::stringstream ss;
            ss << "Position closed: " << symbol << " P/L=" << pos.profit_loss;
            LOG_INFO(ss.str());
            positions_.erase(it);
            return;
        }
        
        pos.avg_price = total_cost / pos.quantity;
        pos.cost = pos.avg_price * pos.quantity;
        pos.market_value = pos.current_price * pos.quantity;
        calculateProfitLoss(pos);
        
        std::stringstream ss;
        ss << "Position updated: " << symbol << " qty=" << pos.quantity 
           << " avg_price=" << pos.avg_price;
        LOG_INFO(ss.str());
    }
}

void PositionManager::updateMarketPrice(const std::string& symbol, double price) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = positions_.find(symbol);
    if (it != positions_.end()) {
        Position& pos = it->second;
        pos.current_price = price;
        pos.market_value = price * pos.quantity;
        calculateProfitLoss(pos);
    }
}

Position* PositionManager::getPosition(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = positions_.find(symbol);
    if (it != positions_.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::map<std::string, Position> PositionManager::getAllPositions() {
    std::lock_guard<std::mutex> lock(mutex_);
    return positions_;
}

int PositionManager::getTotalPositions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return positions_.size();
}

double PositionManager::getTotalMarketValue() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    double total = 0.0;
    for (const auto& pair : positions_) {
        total += pair.second.market_value;
    }
    return total;
}

double PositionManager::getTotalProfitLoss() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    double total = 0.0;
    for (const auto& pair : positions_) {
        total += pair.second.profit_loss;
    }
    return total;
}

bool PositionManager::hasPosition(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return positions_.find(symbol) != positions_.end();
}

void PositionManager::clearPositions() {
    std::lock_guard<std::mutex> lock(mutex_);
    positions_.clear();
    LOG_INFO("All positions cleared");
}

void PositionManager::calculateProfitLoss(Position& position) {
    position.profit_loss = position.market_value - position.cost;
    if (position.cost != 0) {
        position.profit_loss_ratio = position.profit_loss / position.cost;
    } else {
        position.profit_loss_ratio = 0.0;
    }
}

