#include "managers/strategy_manager.h"
#include "managers/position_manager.h"
#include "strategies/strategy_base.h"
#include "strategies/momentum_strategy.h"
#include "event/event.h"
#include "utils/logger.h"
#include <algorithm>
#include <sstream>

StrategyManager& StrategyManager::getInstance() {
    static StrategyManager instance;
    return instance;
}

void StrategyManager::processScanResults(const std::vector<ScanResult>& results) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::stringstream ss;
    ss << "Processing scan results: " << results.size() << " stocks";
    LOG_INFO(ss.str());
    
    // 1. 构建当前扫描的股票代码集合
    std::set<std::string> current_scan_stocks;
    for (const auto& result : results) {
        current_scan_stocks.insert(result.symbol);
    }
    
    // 2. 为新股票创建策略实例（已存在的不重复创建）
    for (const auto& result : results) {
        if (!hasStrategyInstance(result.symbol)) {
            createStrategyInstance(result.symbol, result);
        } else {
            // 更新已存在策略的扫描结果
            auto& instance = strategy_instances_[result.symbol];
            if (instance.strategy && instance.strategy->isRunning()) {
                instance.strategy->onScanResult(result);
            }
        }
    }
    
    // 3. 检查需要删除的策略（不在最新扫描结果中的股票）
    std::vector<std::string> to_remove;
    for (const auto& pair : strategy_instances_) {
        const std::string& symbol = pair.first;
        
        // 如果股票不在当前扫描结果中，考虑删除
        if (current_scan_stocks.find(symbol) == current_scan_stocks.end()) {
            to_remove.push_back(symbol);
        }
    }
    
    // 4. 删除不再符合条件的策略（需要判断是否有持仓）
    for (const auto& symbol : to_remove) {
        removeStrategyInstance(symbol, false);
    }
    
    // 5. 更新上次扫描的股票集合
    last_scan_stocks_ = current_scan_stocks;
    
    ss.str("");
    ss << "Strategy instances: Active=" << getActiveStrategyCount() 
       << ", Total=" << strategy_instances_.size();
    LOG_INFO(ss.str());
}

void StrategyManager::createStrategyInstance(const std::string& symbol, const ScanResult& scan_result) {
    // 不需要锁，调用者已加锁
    
    if (hasStrategyInstance(symbol)) {
        std::stringstream ss;
        ss << "Strategy instance already exists for " << symbol;
        LOG_WARNING(ss.str());
        return;
    }
    
    // 创建新的策略实例
    auto strategy = createStrategy(symbol, scan_result);
    if (!strategy) {
        std::stringstream ss;
        ss << "Failed to create strategy for " << symbol;
        LOG_ERROR(ss.str());
        return;
    }
    
    // 启动策略
    strategy->start();
    
    // 传递扫描结果
    strategy->onScanResult(scan_result);
    
    // 保存实例
    StrategyInstance instance;
    instance.symbol = symbol;
    instance.strategy = strategy;
    instance.is_active = true;
    
    strategy_instances_[symbol] = instance;
    
    std::stringstream ss;
    ss << "Created strategy instance for " << symbol 
       << " (" << scan_result.stock_name << ")"
       << " - Score: " << scan_result.score
       << ", Price: " << scan_result.price
       << ", Change: " << (scan_result.change_ratio * 100) << "%";
    LOG_INFO(ss.str());
}

void StrategyManager::removeStrategyInstance(const std::string& symbol, bool force) {
    // 不需要锁，调用者已加锁
    
    auto it = strategy_instances_.find(symbol);
    if (it == strategy_instances_.end()) {
        return;
    }
    
    // 如果不是强制删除，需要检查是否可以删除
    if (!force && !canRemoveStrategy(symbol)) {
        std::stringstream ss;
        ss << "Cannot remove strategy for " << symbol 
           << " - has active position, will keep monitoring";
        LOG_WARNING(ss.str());
        
        // 标记为不活跃，但保留策略实例继续监控持仓
        it->second.is_active = false;
        return;
    }
    
    // 停止策略
    if (it->second.strategy) {
        it->second.strategy->stop();
    }
    
    // 删除实例
    strategy_instances_.erase(it);
    
    std::stringstream ss;
    ss << "Removed strategy instance for " << symbol;
    LOG_INFO(ss.str());
}

bool StrategyManager::hasStrategyInstance(const std::string& symbol) const {
    // 不需要锁，调用者已加锁
    return strategy_instances_.find(symbol) != strategy_instances_.end();
}

void StrategyManager::startAllStrategies() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : strategy_instances_) {
        if (pair.second.strategy && !pair.second.strategy->isRunning()) {
            pair.second.strategy->start();
        }
    }
    
    std::stringstream ss;
    ss << "Started all strategy instances: " << strategy_instances_.size();
    LOG_INFO(ss.str());
}

void StrategyManager::stopAllStrategies() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : strategy_instances_) {
        if (pair.second.strategy && pair.second.strategy->isRunning()) {
            pair.second.strategy->stop();
        }
    }
    
    std::stringstream ss;
    ss << "Stopped all strategy instances: " << strategy_instances_.size();
    LOG_INFO(ss.str());
}

size_t StrategyManager::getActiveStrategyCount() const {
    // 不需要锁，调用者已加锁
    
    return std::count_if(strategy_instances_.begin(), strategy_instances_.end(),
        [](const std::pair<std::string, StrategyInstance>& pair) {
            return pair.second.strategy && pair.second.strategy->isRunning();
        });
}

std::vector<std::string> StrategyManager::getStrategyStockCodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> codes;
    for (const auto& pair : strategy_instances_) {
        codes.push_back(pair.first);
    }
    return codes;
}

void StrategyManager::printStrategyStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (strategy_instances_.empty()) {
        LOG_INFO("No active strategy instances");
        return;
    }
    
    std::stringstream ss;
    ss << "\n=== Strategy Instances (" << strategy_instances_.size() << ") ===";
    
    for (const auto& pair : strategy_instances_) {
        const auto& instance = pair.second;
        ss << "\n  " << instance.symbol 
           << " - " << (instance.strategy->isRunning() ? "RUNNING" : "STOPPED")
           << " - " << (instance.is_active ? "ACTIVE" : "INACTIVE");
    }
    
    LOG_INFO(ss.str());
}

bool StrategyManager::canRemoveStrategy(const std::string& symbol) const {
    // 不需要锁，调用者已加锁
    
    // 检查是否有持仓
    auto& pos_mgr = PositionManager::getInstance();
    if (pos_mgr.hasPosition(symbol)) {
        return false;  // 有持仓，不能删除
    }
    
    return true;  // 没有持仓，可以删除
}

std::shared_ptr<StrategyBase> StrategyManager::createStrategy(
    const std::string& symbol, 
    const ScanResult& scan_result) {
    
    // 这里可以根据配置或条件选择不同的策略类型
    // 目前默认创建动量策略
    
    auto strategy = std::make_shared<MomentumStrategy>();
    
    // 设置策略名称（包含股票代码）
    std::stringstream ss;
    ss << "Momentum_" << symbol << "_" << scan_result.stock_name;
    // 注意：需要在StrategyBase中添加setName方法，或在构造时传入
    
    return strategy;
}

// ========== 事件处理 ==========

void StrategyManager::initializeEventHandlers(IEventEngine* event_engine) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (event_engine == nullptr) {
        LOG_ERROR("Event engine is null");
        return;
    }
    
    event_engine_ = event_engine;
    
    // 注册事件处理器
    // 使用 lambda 捕获 this，当事件到达时调用对应的处理方法
    kline_handler_id_ = event_engine_->registerHandler(
        EventType::EVENT_KLINE,
        [this](const EventPtr& event) { this->onKLineEvent(event); }
    );
    
    tick_handler_id_ = event_engine_->registerHandler(
        EventType::EVENT_TICK,
        [this](const EventPtr& event) { this->onTickEvent(event); }
    );
    
    trade_handler_id_ = event_engine_->registerHandler(
        EventType::EVENT_TRADE_DEAL,
        [this](const EventPtr& event) { this->onTradeEvent(event); }
    );
    
    LOG_INFO("StrategyManager event handlers registered");
}

void StrategyManager::onKLineEvent(const EventPtr& event) {
    if (!event) {
        return;
    }
    
    // 从事件中提取 KlineData
    const KlineData* kline = event->getData<KlineData>();
    if (kline == nullptr) {
        LOG_ERROR("Failed to extract KlineData from event");
        return;
    }
    
    std::string symbol = kline->symbol;
    
    // 查找对应的策略实例并调用处理方法
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = strategy_instances_.find(symbol);
        if (it == strategy_instances_.end()) {
            // 没有对应的策略实例，直接返回
            return;
        }
        
        if (!it->second.is_active || !it->second.strategy) {
            return;
        }
        
        // 调用策略的 onKLine 方法处理数据
        it->second.strategy->onKLine(symbol, *kline);
    }
}

void StrategyManager::onTickEvent(const EventPtr& event) {
    if (!event) {
        return;
    }
    
    // 从事件中提取 TickData
    const TickData* tick = event->getData<TickData>();
    if (tick == nullptr) {
        LOG_ERROR("Failed to extract TickData from event");
        return;
    }
    
    std::string symbol = tick->symbol;
    
    // 查找对应的策略实例并调用处理方法
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = strategy_instances_.find(symbol);
        if (it == strategy_instances_.end()) {
            // 没有对应的策略实例，直接返回
            return;
        }
        
        if (!it->second.is_active || !it->second.strategy) {
            return;
        }
        
        // 调用策略的 onTick 方法处理数据
        it->second.strategy->onTick(symbol, *tick);
    }
}

void StrategyManager::onTradeEvent(const EventPtr& event) {
    if (!event) {
        return;
    }
    
    // 从事件中提取 TradeData
    const TradeData* trade = event->getData<TradeData>();
    if (trade == nullptr) {
        LOG_ERROR("Failed to extract TradeData from event");
        return;
    }
    
    std::string symbol = trade->symbol;
    
    // 查找对应的策略实例
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = strategy_instances_.find(symbol);
        if (it == strategy_instances_.end()) {
            // 没有对应的策略实例，直接返回
            return;
        }
        
        if (!it->second.is_active || !it->second.strategy) {
            return;
        }
        
        // 可以在这里添加对成交数据的处理
        // 可选：记录成交事件、更新持仓、计算收益等
        std::stringstream ss;
        ss << "Trade executed for " << symbol 
           << " - Direction: " << (trade->direction == Direction::LONG ? "LONG" : "SHORT")
           << ", Volume: " << trade->volume 
           << ", Price: " << trade->price;
        LOG_INFO(ss.str());
        
        // TODO: 可以添加 onTrade() 方法到策略基类中处理成交事件
    }
}

