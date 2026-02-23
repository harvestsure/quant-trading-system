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

// Global flag for graceful shutdown
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
    
    // Display strategy instances
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
    
    // Display position details
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
    
    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Load configuration file (default JSON format)
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
    
    // Display exchange configuration information
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
    
    // Start event engine (must be started before other modules)
    auto& event_engine = EventEngine::getInstance();
    auto log_event_handler = std::bind(&Logger::handld_logs, &Logger::getInstance(), std::placeholders::_1);
    event_engine.registerHandler(EventType::EVENT_LOG, log_event_handler);
   
    event_engine.start();
    LOG_INFO("Event engine started");
    
    // Initialize exchanges
    auto& exchange_mgr = ExchangeManager::getInstance();
    exchange_mgr.setEventEngine(&event_engine);
    
    // Initialize all enabled exchanges
    if (!exchange_mgr.initAllExchanges(config.exchanges)) {
        LOG_ERROR("Failed to initialize exchanges");
        return 1;
    }
    
    // Connect to all exchanges
    auto exchanges = exchange_mgr.getAllExchanges();
    for (auto& exchange : exchanges) {
        if (!exchange->connect()) {
            LOG_WARN("Failed to connect to exchange: " + exchange->getName());
        } else {
            LOG_INFO("Connected to exchange: " + exchange->getName());
        }
    }
    
    // Initialize strategy manager (strategy instances will be created dynamically by scanner)
    auto& strategy_mgr = StrategyManager::getInstance();
    
    // Initialize event handlers for strategy manager
    strategy_mgr.initializeEventHandlers(&event_engine);
    LOG_INFO("Strategy manager initialized - strategies will be created dynamically based on scan results");
    
    // Create and start market scanner
    // Scanner will automatically notify strategy manager to create/delete strategy instances
    MarketScanner scanner;
    
    // Add all connected exchanges to scanner
    for (auto& exchange : exchanges) {
        if (exchange->isConnected()) {
            scanner.addExchange(exchange);
        }
    }
    
    scanner.start();
    LOG_INFO("Market scanner started - will create strategy instances for qualified stocks");
    
    LOG_INFO("\nSystem is running. Press Ctrl+C to stop.\n");
    LOG_INFO("Status updates will be printed every minute.\n\n");
    
    // Main loop
    int status_counter = 0;
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        status_counter++;
        
        // Print status every minute
        if (status_counter >= 60) {
            printSystemStatus();
            status_counter = 0;
        }
    }
    
    // Graceful shutdown
    LOG_INFO("\nShutting down system...\n");
    
    // Stop scanner
    scanner.stop();
    LOG_INFO("Market scanner stopped");
    
    // Stop all strategies
    strategy_mgr.stopAllStrategies();
    LOG_INFO("All strategies stopped");
    
    // Print final status
    printSystemStatus();
    
    // Disconnect all exchanges
    exchanges = exchange_mgr.getAllExchanges();
    for (auto& exchange : exchanges) {
        exchange->disconnect();
        LOG_INFO("Disconnected from exchange: " + exchange->getName());
    }
    
    // Stop event engine (stop last)
    event_engine.stop();
    LOG_INFO("Event engine stopped");
    
    LOG_INFO("=== Quant Trading System Stopped ===");
    
    LOG_INFO("\nSystem stopped successfully.\n");
    
    return 0;
}
