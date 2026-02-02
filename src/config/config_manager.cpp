#include "config/config_manager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>

using json = nlohmann::json;

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadFromFile(const std::string& config_file) {
    // 根据文件扩展名判断格式
    if (config_file.substr(config_file.find_last_of(".") + 1) == "json") {
        return loadFromJson(config_file);
    } else {
        return loadFromText(config_file);
    }
}

bool ConfigManager::loadFromJson(const std::string& json_file) {
    try {
        std::ifstream file(json_file);
        if (!file.is_open()) {
            std::cerr << "Failed to open JSON file: " << json_file << std::endl;
            return false;
        }
        
        json j;
        file >> j;
        parseJsonConfig(j);
        
        std::cout << "Config loaded successfully from JSON: " << json_file << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load JSON config: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::loadFromText(const std::string& text_file) {
    std::ifstream file(text_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << text_file << std::endl;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // 跳过注释和空行
        if (line.empty() || line[0] == '#') continue;
        parseConfigLine(line);
    }
    
    file.close();
    std::cout << "Config loaded successfully from text: " << text_file << std::endl;
    return true;
}

void ConfigManager::parseJsonConfig(const json& j) {
    // 解析多交易所配置
    if (j.contains("exchange") && j["exchange"].is_object()) {
		const auto& exch_array = j["exchange"];
        for (auto& [key, exch] : exch_array.items()) {

            ExchangeInstanceConfig instance;
            instance.name = key;
            instance.is_enabled = exch.value("is_enabled", false);
            instance.is_simulation = exch.value("is_simulation", true);
            instance.params = exch.value("params", json::object());

            if (!instance.name.empty()) {
                config_.exchanges.push_back(instance);
            }
        }
    }
    
    // 解析交易参数
    if (j.contains("trading")) {
        const auto& trading = j["trading"];
        config_.trading.max_position_size = trading.value("max_position_size", 100000.0);
        config_.trading.single_stock_max_ratio = trading.value("single_stock_max_ratio", 0.2);
        config_.trading.max_positions = trading.value("max_positions", 10);
    }
    
    // 解析扫描参数
    if (j.contains("scanner")) {
        const auto& scanner = j["scanner"];
        config_.scanner.interval_minutes = scanner.value("interval_minutes", 5);
        config_.scanner.min_price = scanner.value("min_price", 1.0);
        config_.scanner.max_price = scanner.value("max_price", 1000.0);
        config_.scanner.min_volume = scanner.value("min_volume", 1000000.0);
        config_.scanner.min_turnover_rate = scanner.value("min_turnover_rate", 0.01);
        config_.scanner.top_n = scanner.value("top_n", 10);
    }
    
    // 解析风险管理参数
    if (j.contains("risk")) {
        const auto& risk = j["risk"];
        config_.risk.stop_loss_ratio = risk.value("stop_loss_ratio", 0.05);
        config_.risk.take_profit_ratio = risk.value("take_profit_ratio", 0.15);
        config_.risk.max_daily_loss = risk.value("max_daily_loss", 0.03);
        config_.risk.trailing_stop_ratio = risk.value("trailing_stop_ratio", 0.03);
        config_.risk.max_drawdown = risk.value("max_drawdown", 0.1);
    }
    
    // 解析策略参数
    if (j.contains("strategy")) {
        const auto& strategy = j["strategy"];
        if (strategy.contains("momentum")) {
            const auto& momentum = strategy["momentum"];
            config_.strategy.momentum.enabled = momentum.value("enabled", true);
            config_.strategy.momentum.rsi_period = momentum.value("rsi_period", 14);
            config_.strategy.momentum.rsi_oversold = momentum.value("rsi_oversold", 30);
            config_.strategy.momentum.rsi_overbought = momentum.value("rsi_overbought", 70);
            config_.strategy.momentum.ma_period = momentum.value("ma_period", 20);
            config_.strategy.momentum.volume_factor = momentum.value("volume_factor", 1.5);
        }
    }
    
    // 解析日志配置
    if (j.contains("logging")) {
        const auto& logging = j["logging"];
        config_.logging.level = logging.value("level", "INFO");
        config_.logging.console = logging.value("console", true);
        config_.logging.file = logging.value("file", true);
        config_.logging.file_dir = logging.value("file_dir", "logs");
    }
}

void ConfigManager::parseConfigLine(const std::string& line) {
    std::istringstream iss(line);
    std::string key, value;
    
    if (std::getline(iss, key, '=') && std::getline(iss, value)) {
        // 去除空格
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
    }
}

std::vector<ExchangeInstanceConfig> ConfigManager::getEnabledExchanges() const {
    std::vector<ExchangeInstanceConfig> enabled;
    for (const auto& exch : config_.exchanges) {
        if (exch.is_enabled) {
            enabled.push_back(exch);
        }
    }
    return enabled;
}

const ExchangeInstanceConfig* ConfigManager::getExchange(const std::string& name) const {
    for (const auto& exch : config_.exchanges) {
        if (exch.name == name && exch.is_enabled) {
            return &exch;
        }
    }
    return nullptr;
}

