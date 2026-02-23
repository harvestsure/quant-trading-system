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
    
    // Update position
    void updatePosition(const std::string& symbol, int quantity, double price);

    // Update market price
    void updateMarketPrice(const std::string& symbol, double price);

    // Get position
    Position* getPosition(const std::string& symbol);
    std::map<std::string, Position> getAllPositions();

    // Position queries
    int getTotalPositions() const;
    double getTotalMarketValue() const;
    double getTotalProfitLoss() const;
    bool hasPosition(const std::string& symbol) const;

    // Clear positions (for testing)
    void clearPositions();

    // Non-copyable
    PositionManager(const PositionManager&) = delete;
    PositionManager& operator=(const PositionManager&) = delete;
    
private:
    PositionManager() = default;
    
    std::map<std::string, Position> positions_;
    mutable std::mutex mutex_;
    
    void calculateProfitLoss(Position& position);
};

 
