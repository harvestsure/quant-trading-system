#include "config/config_manager.h"
#include "utils/logger.h"
#include "managers/position_manager.h"
#include "managers/risk_manager.h"
#include "managers/strategy_manager.h"
#include "scanner/market_scanner.h"
#include "exchange/exchange_manager.h"
#include "exchange/exchange_interface.h"
#include "event/event_engine.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>

// 全局标志用于优雅退出
std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived shutdown signal, stopping system..." << std::endl;
        g_running = false;
    }
}

void printSystemStatus() {
    auto& pos_mgr = PositionManager::getInstance();
    auto& risk_mgr = RiskManager::getInstance();
    auto& strategy_mgr = StrategyManager::getInstance();
    
    std::cout << "\n========== System Status ==========\n";
    std::cout << "Active Strategies: " << strategy_mgr.getActiveStrategyCount() << "\n";
    
    // 显示策略实例
    auto strategy_stocks = strategy_mgr.getStrategyStockCodes();
    if (!strategy_stocks.empty()) {
        std::cout << "Strategy Instances: ";
        for (size_t i = 0; i < strategy_stocks.size(); ++i) {
            std::cout << strategy_stocks[i];
            if (i < strategy_stocks.size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
    }
    
    std::cout << "Total Positions: " << pos_mgr.getTotalPositions() << "\n";
    std::cout << "Total Market Value: $" << pos_mgr.getTotalMarketValue() << "\n";
    std::cout << "Total P/L: $" << pos_mgr.getTotalProfitLoss() << "\n";
    
    auto metrics = risk_mgr.getRiskMetrics();
    std::cout << "Daily P/L: $" << metrics.daily_pnl 
              << " (" << (metrics.daily_pnl_ratio * 100) << "%)\n";
    std::cout << "Total Trades: " << metrics.total_trades 
              << " (Win: " << metrics.winning_trades 
              << ", Loss: " << metrics.losing_trades << ")\n";
    
    // 显示持仓详情
    auto positions = pos_mgr.getAllPositions();
    if (!positions.empty()) {
        std::cout << "\n--- Current Positions ---\n";
        for (const auto& pair : positions) {
            const auto& pos = pair.second;
            std::cout << pos.symbol << ": " 
                     << pos.quantity << " @ $" << pos.avg_price
                     << " (Current: $" << pos.current_price
                     << ", P/L: $" << pos.profit_loss 
                     << " " << (pos.profit_loss_ratio * 100) << "%)\n";
        }
    }
    
    std::cout << "===================================\n\n";
}

int main(int argc, char* argv[]) {
    std::cout << "===================================\n";
    std::cout << "  Quant Trading System v1.0\n";
    std::cout << "===================================\n\n";
    
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 加载配置文件（默认使用JSON格式）
    std::string config_file = "config.json";
    if (argc > 1) {
        config_file = argv[1];
    }
    
    auto& config_mgr = ConfigManager::getInstance();
    if (!config_mgr.loadFromFile(config_file)) {
        std::cerr << "Failed to load config file: " << config_file << std::endl;
        return 1;
    }
    
    const auto& config = config_mgr.getConfig();
    
    // 显示交易所配置信息
    std::cout << "Enabled Exchanges:\n";
    for (const auto& exch : config.exchanges) {
        std::cout << "  - " << exch.name 
                  << " (Enabled: " << (exch.is_enabled ? "Yes" : "No")
                  << ", Mode: " << (exch.is_simulation ? "SIMULATION" : "LIVE") << ")\n";
    }
    std::cout << "Scan Interval: " << config.scanner.interval_minutes << " minutes\n";
    std::cout << "Max Position Size: $" << config.trading.max_position_size << "\n";
    std::cout << "Max Positions: " << config.trading.max_positions << "\n\n";
    
    LOG_INFO("=== Quant Trading System Started ===");
    
    // 启动事件引擎（必须在其他模块之前启动）
    auto& event_engine = EventEngine::getInstance();
    auto log_event_handler = std::bind(&Logger::handld_logs, &Logger::getInstance(), std::placeholders::_1);
    event_engine.registerHandler(EventType::EVENT_LOG, log_event_handler);
   
    event_engine.start();
    LOG_INFO("Event engine started");
    
    // 初始化交易所
    auto& exchange_mgr = ExchangeManager::getInstance();
    exchange_mgr.setEventEngine(&event_engine);
    
    // 初始化所有启用的交易所
    if (!exchange_mgr.initAllExchanges(config.exchanges)) {
        LOG_ERROR("Failed to initialize exchanges");
        return 1;
    }
    
    // 连接所有交易所
    auto exchanges = exchange_mgr.getAllExchanges();
    for (auto& exchange : exchanges) {
        if (!exchange->connect()) {
            LOG_WARNING("Failed to connect to exchange: " + exchange->getName());
        } else {
            LOG_INFO("Connected to exchange: " + exchange->getName());
        }
    }
    
    // 初始化策略管理器（策略实例将由扫描器动态创建）
    auto& strategy_mgr = StrategyManager::getInstance();
    LOG_INFO("Strategy manager initialized - strategies will be created dynamically based on scan results");
    
    // 创建并启动市场扫描器
    // 扫描器会自动通知策略管理器创建/删除策略实例
    MarketScanner scanner;
    
    // 添加所有已连接的交易所到扫描器
    for (auto& exchange : exchanges) {
        if (exchange->isConnected()) {
            scanner.addExchange(exchange);
        }
    }
    
    scanner.start();
    LOG_INFO("Market scanner started - will create strategy instances for qualified stocks");
    
    LOG_INFO("\nSystem is running. Press Ctrl+C to stop.\n");
    LOG_INFO("Status updates will be printed every minute.\n\n");
    
    // 主循环
    int status_counter = 0;
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        status_counter++;
        
        // 每分钟打印一次状态
        if (status_counter >= 60) {
            printSystemStatus();
            status_counter = 0;
        }
    }
    
    // 优雅退出
    LOG_INFO("\nShutting down system...\n");
    
    // 停止扫描器
    scanner.stop();
    LOG_INFO("Market scanner stopped");
    
    // 停止所有策略
    strategy_mgr.stopAllStrategies();
    LOG_INFO("All strategies stopped");
    
    // 打印最终状态
    printSystemStatus();
    
    // 断开所有交易所连接
    exchanges = exchange_mgr.getAllExchanges();
    for (auto& exchange : exchanges) {
        exchange->disconnect();
        LOG_INFO("Disconnected from exchange: " + exchange->getName());
    }
    
    // 停止事件引擎（最后停止）
    event_engine.stop();
    LOG_INFO("Event engine stopped");
    
    LOG_INFO("=== Quant Trading System Stopped ===");
    
    LOG_INFO("\nSystem stopped successfully.\n");
    
    return 0;
}
