#pragma once

#include "strategies/strategy_base.h"
#include <map>
#include <vector>
#include <chrono>
#include <mutex>

// 追涨杀跌动量策略 - 检测爆发个股并追涨，实时动态止盈止损
class MomentumStrategy : public StrategyBase {
public:
    MomentumStrategy();
    ~MomentumStrategy() override = default;
    
    void onScanResult(const ScanResult& result) override;
    void onKLine(const std::string& symbol, const KlineData& kline) override;
    void onTick(const std::string& symbol, const TickData& tick) override;
    void onSnapshot(const Snapshot& snapshot) override;
    
private:
    // 追涨跟踪信息
    struct ChaseEntry {
        double entry_price = 0.0;          // 入场价
        double high_water_mark = 0.0;      // 入场后最高价(用于追踪止盈)
        double entry_volume_ratio = 0.0;   // 入场时量比
        double entry_score = 0.0;          // 入场时爆发评分
        int64_t entry_time_ms = 0;         // 入场时间戳
    };
    
    std::map<std::string, ChaseEntry> chase_entries_;  // 追涨持仓跟踪
    mutable std::mutex chase_mutex_;
    
    // 技术指标计算
    double calculateRSI(const std::vector<KlineData>& klines, int period = 14);
    double calculateMACD(const std::vector<KlineData>& klines);
    bool isUptrend(const std::vector<KlineData>& klines);
    
    // 追涨杀跌决策
    bool shouldEnter(const ScanResult& result, const std::vector<KlineData>& klines);
    bool shouldChaseExit(const std::string& symbol, double current_price, double speed);
    
    // 仓位计算
    int calculateQuantity(const std::string& symbol, double price);
    
    // 工具方法
    int64_t currentTimeMs() const;
};

 
