#pragma once

#include <string>
#include <map>
#include <mutex>
#include <memory>

 

struct Position {
    std::string symbol;
    int quantity;
    double avg_price;
    double current_price;
    double cost;
    double market_value;
    double profit_loss;
    double profit_loss_ratio;
    std::string side;  // "LONG" or "SHORT"
};

class PositionManager {
public:
    static PositionManager& getInstance();
    
    // 更新持仓
    void updatePosition(const std::string& symbol, int quantity, double price);
    
    // 更新市价
    void updateMarketPrice(const std::string& symbol, double price);
    
    // 获取持仓
    Position* getPosition(const std::string& symbol);
    std::map<std::string, Position> getAllPositions();
    
    // 持仓查询
    int getTotalPositions() const;
    double getTotalMarketValue() const;
    double getTotalProfitLoss() const;
    bool hasPosition(const std::string& symbol) const;
    
    // 清空持仓（用于测试）
    void clearPositions();
    
    // 禁止拷贝
    PositionManager(const PositionManager&) = delete;
    PositionManager& operator=(const PositionManager&) = delete;
    
private:
    PositionManager() = default;
    
    std::map<std::string, Position> positions_;
    mutable std::mutex mutex_;
    
    void calculateProfitLoss(Position& position);
};

 
