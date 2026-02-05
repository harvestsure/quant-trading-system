#pragma once

#include "strategies/strategy_base.h"
#include <map>
#include <vector>

 

// 追涨杀跌动量策略
class MomentumStrategy : public StrategyBase {
public:
    MomentumStrategy();
    ~MomentumStrategy() override = default;
    
    void onScanResult(const ScanResult& result) override;
    void onKLine(const std::string& symbol, const KlineData& kline) override;
    void onTick(const std::string& symbol, const TickData& tick) override;
    void onSnapshot(const Snapshot& snapshot) override;
    
private:
    // 持仓监控
    std::map<std::string, double> entry_prices_;  // 入场价格
    
    // 技术指标计算
    double calculateRSI(const std::vector<KlineData>& klines, int period = 14);
    double calculateMACD(const std::vector<KlineData>& klines);
    bool isUptrend(const std::vector<KlineData>& klines);
    
    // 交易决策
    bool shouldEnter(const ScanResult& result, const std::vector<KlineData>& klines);
    bool shouldExit(const std::string& symbol, double current_price);
    
    // 仓位计算
    int calculateQuantity(const std::string& symbol, double price);
};

 
