#include "scanner/market_scanner.h"
#include "managers/strategy_manager.h"
#include "config/config_manager.h"
#include "utils/logger.h"
#include <chrono>
#include <thread>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <cmath>
#include <sstream>

MarketScanner::MarketScanner() : running_(false) {
    LOG_INFO("Market scanner initialized");
}

MarketScanner::~MarketScanner() {
    stop();
}

void MarketScanner::addExchange(std::shared_ptr<IExchange> exchange) {
    std::lock_guard<std::mutex> lock(exchanges_mutex_);
    if (exchange) {
        exchanges_.push_back(exchange);
        LOG_INFO("Exchange added: " + exchange->getName());
    }
}

void MarketScanner::start() {
    if (running_) {
        LOG_WARN("Market scanner already running");
        return;
    }
    
    // Load scanner parameters from the configuration file
    scanner_params_ = ConfigManager::getInstance().getScannerParams();
    LOG_INFO("Loaded scanner config - top_n: " + std::to_string(scanner_params_.top_n) +
             ", breakout_vol_ratio: " + std::to_string(scanner_params_.breakout_volume_ratio_min) +
             ", breakout_change: [" + std::to_string(scanner_params_.breakout_change_ratio_min) + 
             ", " + std::to_string(scanner_params_.breakout_change_ratio_max) + "]");
    
    std::lock_guard<std::mutex> lock(exchanges_mutex_);
    if (exchanges_.empty()) {
        LOG_ERROR("No exchanges configured");
        return;
    }
    
    running_ = true;
    scan_thread_ = std::make_unique<std::thread>(&MarketScanner::scanLoop, this);
    
    LOG_INFO("Market scanner started with " + std::to_string(exchanges_.size()) + " exchange(s)");
}

void MarketScanner::stop() {
    if (!running_) return;
    
    running_ = false;
    if (scan_thread_ && scan_thread_->joinable()) {
        scan_thread_->join();
    }
    
    LOG_INFO("Market scanner stopped");
}

void MarketScanner::setWatchList(const std::string& exchange_name, const std::vector<std::string>& watch_list) {
    std::lock_guard<std::mutex> lock(watch_list_mutex_);
    watch_lists_[exchange_name] = watch_list;
    
    LOG_INFO("Watch list set for " + exchange_name + ": " + std::to_string(watch_list.size()) + " stocks");
}

void MarketScanner::clearWatchLists() {
    std::lock_guard<std::mutex> lock(watch_list_mutex_);
    watch_lists_.clear();
    LOG_INFO("All watch lists cleared");
}

MarketScanner::ScannerStatus MarketScanner::getStatus() const {
    std::lock_guard<std::mutex> watch_lock(watch_list_mutex_);
    std::lock_guard<std::mutex> qualified_lock(qualified_stocks_mutex_);
    std::lock_guard<std::mutex> exch_lock(exchanges_mutex_);
    
    std::vector<std::string> active_exchanges;
    std::map<std::string, int> watch_counts;
    
    for (const auto& exch : exchanges_) {
        if (exch && exch->isConnected()) {
            active_exchanges.push_back(exch->getName());
            auto it = watch_lists_.find(exch->getName());
            if (it != watch_lists_.end()) {
                watch_counts[exch->getName()] = it->second.size();
            }
        }
    }
    
    return {
        running_,
        watch_counts,
        qualified_stocks_,
        isInTradingTime(),
        isInOpeningPeriod(),
        active_exchanges
    };
}

void MarketScanner::scanLoop() {
    // Initialize watch lists for each exchange (fetched from the exchange)
    {
        std::lock_guard<std::mutex> lock(watch_list_mutex_);
        std::lock_guard<std::mutex> exch_lock(exchanges_mutex_);
        
        for (const auto& exchange : exchanges_) {
            if (!exchange || !exchange->isConnected()) {
                continue;
            }
            
            const auto& exch_name = exchange->getName();
            if (watch_lists_.find(exch_name) == watch_lists_.end()) {
                auto stock_list = exchange->getMarketStockList();
                    if (!stock_list.empty()) {
                    watch_lists_[exch_name] = stock_list;
                    LOG_INFO("Loaded " + std::to_string(stock_list.size()) + " stocks from " + exch_name);
                    // Volume history will be loaded on-demand during scanning
                    LOG_INFO("Volume history will be loaded on-demand during scanning");
                }
            }
        }
    }
    
    while (running_) {
        try {
            if (/*isInTradingTime()*/ true) {
                // Execute scan for each exchange
                std::lock_guard<std::mutex> exch_lock(exchanges_mutex_);
                for (const auto& exchange : exchanges_) {
                    if (exchange && exchange->isConnected()) {
                        performScan(exchange);
                    }
                }
                
                // Choose scan interval based on the time period
                int interval_ms = isInOpeningPeriod() ? 
                    OPENING_SCAN_INTERVAL_MS : NORMAL_SCAN_INTERVAL_MS;
                
                // Sleep in small increments to respond quickly to stop()
                auto end_time = std::chrono::steady_clock::now() + 
                               std::chrono::milliseconds(interval_ms);
                while (running_ && std::chrono::steady_clock::now() < end_time) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } else {
                // Non-trading period, perform low-frequency checks
                std::this_thread::sleep_for(std::chrono::milliseconds(NON_TRADING_SCAN_INTERVAL_MS));
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("Scan loop error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
}

void MarketScanner::performScan(const std::shared_ptr<IExchange>& exchange) {
    if (!exchange) {
        return;
    }
    
    const auto& exch_name = exchange->getName();
    std::vector<std::string> watch_list;
    
    // Get the watch list for the exchange
    {
        std::lock_guard<std::mutex> lock(watch_list_mutex_);
        auto it = watch_lists_.find(exch_name);
        if (it == watch_lists_.end() || it->second.empty()) {
            LOG_WARN("No watch list for exchange: " + exch_name);
            return;
        }
        watch_list = it->second;
    }
    
    LOG_INFO("Starting breakout scan for " + exch_name + " (" + std::to_string(watch_list.size()) + " stocks)...");
    
    // Fetch market data
    auto results = batchFetchMarketData(exchange, watch_list);
    
    // Filter stocks that meet breakout criteria
    std::vector<ScanResult> filtered_results;
    for (auto& result : results) {
        if (meetsSelectionCriteria(result)) {
            filtered_results.push_back(result);
        }
    }
    
    // Sort by breakout score
    std::sort(filtered_results.begin(), filtered_results.end(),
        [](const ScanResult& a, const ScanResult& b) {
            return a.score > b.score;
        });
    
    // Keep only the top_n results
    if (filtered_results.size() > static_cast<size_t>(scanner_params_.top_n)) {
        filtered_results.resize(scanner_params_.top_n);
    }
    
    // Print breakout stock details
    if (!filtered_results.empty()) {
        std::stringstream ss;
        ss << "\n=== Breakout Scan Results (" << exch_name << ") ===";
        for (size_t i = 0; i < filtered_results.size(); ++i) {
            const auto& r = filtered_results[i];
            ss << "\n  #" << (i + 1) << " " << r.symbol << " " << r.stock_name
               << " | Price: " << r.price
               << " | Chg: " << std::fixed << std::setprecision(2) << (r.change_ratio * 100) << "%"
               << " | VolRatio: " << std::setprecision(1) << r.volume_ratio << "x"
               << " | Amp: " << std::setprecision(2) << (r.amplitude * 100) << "%"
               << " | Speed: " << std::setprecision(2) << (r.speed * 100) << "%"
               << " | Turnover: " << std::setprecision(2) << (r.turnover_rate * 100) << "%"
               << " | B/A: " << std::setprecision(2) << r.bid_ask_ratio
               << " | vsHigh: " << std::setprecision(2) << (r.price_vs_high * 100) << "%"
               << " | Score: " << std::setprecision(1) << r.score;
        }
        LOG_INFO(ss.str());
    }
    
    // Update qualified stocks list
    {
        std::lock_guard<std::mutex> lock(qualified_stocks_mutex_);
        qualified_stocks_[exch_name].clear();
        for (const auto& result : filtered_results) {
            qualified_stocks_[exch_name].push_back(result.symbol);
        }
    }
    
    LOG_INFO("Scan completed for " + exch_name + ": found " + std::to_string(filtered_results.size()) + " breakout stocks");
    
    // Pass results to the StrategyManager
    if (!filtered_results.empty()) {
        StrategyManager::getInstance().processScanResults(filtered_results);
    }
}

std::vector<ScanResult> MarketScanner::batchFetchMarketData(const std::shared_ptr<IExchange>& exchange, const std::vector<std::string>& symbols) {
    std::vector<ScanResult> all_results;
    
    if (!exchange || !exchange->isConnected()) {
        LOG_ERROR("Exchange not connected");
        return all_results;
    }
    
    // Fetch data in batches
    for (size_t i = 0; i < symbols.size(); i += BATCH_SIZE) {
        if (!running_) {
            break;
        }
        
        size_t end_idx = std::min(i + BATCH_SIZE, symbols.size());
        std::vector<std::string> batch(symbols.begin() + i, symbols.begin() + end_idx);
        
        try {
            // Call the exchange's batch snapshot API
            auto snapshots = exchange->getBatchSnapshots(batch);
            
            // Convert to ScanResult and compute breakout metrics and score
            for (const auto& pair : snapshots) {
                ScanResult result = convertSnapshotToScanResult(pair.second, exchange->getName(), exchange);
                result.score = calculateScore(result);
                all_results.push_back(result);
                
                // Update historical data (used for next speed calculation)
                updateVolumeHistory(result.symbol, pair.second.volume, pair.second.last_price);
                
                // Cache the current snapshot (used to calculate speed)
                {
                    std::lock_guard<std::mutex> lock(last_snapshots_mutex_);
                    last_snapshots_[result.symbol] = pair.second;
                }
            }
            
            // Pause between batches to avoid too many requests
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to fetch batch [" + std::to_string(i) + ", " + std::to_string(end_idx) + "): " + std::string(e.what()));
        }
    }
    
    return all_results;
}

ScanResult MarketScanner::convertSnapshotToScanResult(const Snapshot& snapshot, 
                                                       const std::string& exchange_name,
                                                       std::shared_ptr<IExchange> exchange) {
    ScanResult result;
    result.symbol = snapshot.symbol;
    result.stock_name = snapshot.name;
    result.price = snapshot.last_price;
    result.change_ratio = (snapshot.pre_close > 0) ? 
        (snapshot.last_price - snapshot.pre_close) / snapshot.pre_close : 0.0;
    result.volume = snapshot.volume;
    result.turnover_rate = snapshot.turnover_rate;
    result.exchange_name = exchange_name;
    result.exchange = exchange;
    
    // === Breakout detection metric calculations ===
    
    // 1. Volume ratio - lazy-loaded, only calculated for stocks passing basic filters
    // (avoid fetching K-lines for all stocks)
    result.volume_ratio = -1.0;  // mark as not calculated
    
    // 2. Amplitude (intraday volatility)
    if (snapshot.open_price > 0) {
        result.amplitude = (snapshot.high_price - snapshot.low_price) / snapshot.open_price;
    }
    
    // 3. Speed (price change rate compared to last scan)
    result.speed = calculateSpeed(snapshot.symbol, snapshot.last_price);
    
    // 4. Bid/ask volume ratio
    result.bid_ask_ratio = calculateBidAskRatio(snapshot);
    
    // 5. Price information
    result.open_price = snapshot.open_price;
    result.high_price = snapshot.high_price;
    result.low_price = snapshot.low_price;
    result.pre_close = snapshot.pre_close;
    
    // 6. Ratio to daily high (smaller means closer to new high, indicating lifting)
    if (snapshot.high_price > 0) {
        result.price_vs_high = (snapshot.high_price - snapshot.last_price) / snapshot.high_price;
    }
    
    return result;
}

std::pair<int, int> MarketScanner::getCurrentTime() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    return {tm.tm_hour, tm.tm_min};
}

bool MarketScanner::isInTradingTime() const {
    auto time_pair = getCurrentTime();
    int hour = time_pair.first;
    int minute = time_pair.second;
    int current_min = hour * 60 + minute;
    
    // Hong Kong market: 9:30-12:00 (morning), 13:00-16:00 (afternoon)
    int morning_open = 9 * 60 + 30;    // 9:30
    int morning_close = 12 * 60;       // 12:00
    int afternoon_open = 13 * 60;      // 13:00
    int afternoon_close = 16 * 60;     // 16:00
    
    bool is_morning = current_min >= morning_open && current_min <= morning_close;
    bool is_afternoon = current_min >= afternoon_open && current_min <= afternoon_close;
    
    return is_morning || is_afternoon;
}

bool MarketScanner::isInOpeningPeriod() const {
    auto time_pair = getCurrentTime();
    int hour = time_pair.first;
    int minute = time_pair.second;
    
    // Opening periods: first 30 minutes after open (9:30-10:00) and after afternoon open (13:00-13:30)
    // These windows are when breakout stocks are most likely to appear
    bool morning_opening = (hour == 9 && minute >= 30) || (hour == 10 && minute == 0);
    bool afternoon_opening = (hour == 13 && minute < 30);
    return morning_opening || afternoon_opening;
}

bool MarketScanner::meetsSelectionCriteria(ScanResult& result) {
    // ===== Breakout stock selection criteria for HK market =====
    // Core idea: find stocks with sudden volume surge and rapid price increase
    
    // 1. Price filter (avoid penny stocks and extremely large-cap stocks)
    if (result.price < scanner_params_.min_price || result.price > scanner_params_.max_price) {
        return false;
    }
    
    // 2. Change range: must be positive
    //    Too low (<2%) = not strong enough, too high (>10%) = high risk of chasing a peak
    if (result.change_ratio < scanner_params_.breakout_change_ratio_min || 
        result.change_ratio > scanner_params_.breakout_change_ratio_max) {
        return false;
    }
    
    // 3. Volume ratio condition: lazy load - calculate only for stocks passing basic filters
    //    Abnormal volume surge is the core breakout signal
    if (result.volume_ratio < 0) {
        // Lazy-load volume ratio; use exchange pointer in result to avoid extra locking
        result.volume_ratio = calculateVolumeRatio(result.symbol, result.volume, result.exchange);
    }
    if (result.volume_ratio < scanner_params_.breakout_volume_ratio_min) {
        return false;
    }
    
    // 4. Amplitude condition: require volatility to provide trading space
    if (result.amplitude < scanner_params_.breakout_amplitude_min) {
        return false;
    }
    
    // 5. Turnover rate: market attention and liquidity
    if (result.turnover_rate < scanner_params_.min_turnover_rate) {
        return false;
    }
    
    // 6. Absolute volume filter
    if (result.volume < scanner_params_.min_volume) {
        return false;
    }
    
    // 7. Buy/sell force: bids should be stronger than asks
    if (result.bid_ask_ratio < 0.8) {
        return false;  // Selling pressure is too high; not suitable for chasing
    }
    
    // 8. Distance to daily high: avoid stocks that have pulled back significantly
    //    Price near intraday high => major players still pushing up
    if (result.price_vs_high > 0.05) {
        return false;  // More than 5% from high; may have pulled back
    }
    
    return true;
}

double MarketScanner::calculateScore(const ScanResult& result) const {
    // ===== Composite scoring algorithm for HK market breakouts =====
    // Normalize each dimension to [0, 1] and compute weighted sum
    double score = 0.0;
    
    // 1. Volume ratio score (weight 35%): higher is better, diminishing returns after 10x
    double volume_score = std::min(1.0, result.volume_ratio / 10.0);
    score += volume_score * scanner_params_.breakout_score_weight_volume;
    
    // 2. Change score (weight 25%): 3-6% is the sweet spot
    double change_score = 0.0;
    if (result.change_ratio >= 0.03 && result.change_ratio <= 0.06) {
        change_score = 1.0;  // sweet spot
    } else if (result.change_ratio > 0.06) {
        change_score = std::max(0.0, 1.0 - (result.change_ratio - 0.06) / 0.04);
    } else {
        change_score = result.change_ratio / 0.03;
    }
    score += change_score * scanner_params_.breakout_score_weight_change;
    
    // 3. Speed score (weight 25%): faster speed indicates stronger momentum
    double speed_score = std::min(1.0, std::max(0.0, result.speed * 100.0));
    score += speed_score * scanner_params_.breakout_score_weight_speed;
    
    // 4. Turnover score (weight 15%)
    double turnover_score = std::min(1.0, result.turnover_rate / 0.10);
    score += turnover_score * scanner_params_.breakout_score_weight_turnover;
    
    // 5. Bonus: overwhelming bid advantage
    if (result.bid_ask_ratio > 2.0) {
        score += 5.0;
    }
    
    // 6. Bonus: near intraday high (indicates strong buying pressure)
    if (result.price_vs_high < 0.01) {
        score += 5.0;
    }
    
    // 7. Opening period bonus (breakouts at open are likelier to sustain)
    if (isInOpeningPeriod()) {
        score *= 1.1;
    }
    
    return score;
}

// ===== Core breakout detection methods =====

double MarketScanner::calculateVolumeRatio(const std::string& symbol, int64_t current_volume, 
                                            const std::shared_ptr<IExchange>& exchange) {
    // Check for existing historical data; lazy-load if missing
    bool need_load = false;
    {
        std::lock_guard<std::mutex> lock(volume_history_mutex_);
        auto it = volume_history_.find(symbol);
        if (it == volume_history_.end() || it->second.avg_volume <= 0) {
            need_load = true;
        }
    }
    
    if (need_load && exchange && exchange->isConnected()) {
            // Lazy load: fetch daily K-line when first encountering a symbol
        try {
            auto klines = exchange->getHistoryKLine(symbol, "K_DAY", VOLUME_HISTORY_DAYS + 1);
            if (klines.size() >= 2) {
                std::lock_guard<std::mutex> lock(volume_history_mutex_);
                auto& history = volume_history_[symbol];
                history.daily_volumes.clear();
                for (size_t j = 0; j < klines.size() - 1; ++j) {
                    history.daily_volumes.push_back(klines[j].volume);
                }
                int64_t total = 0;
                for (auto v : history.daily_volumes) total += v;
                history.avg_volume = total / (int64_t)history.daily_volumes.size();
                history.last_price = klines.back().close_price;
            }
        } catch (...) {}
    }
    
    // Calculate volume ratio
    std::lock_guard<std::mutex> lock(volume_history_mutex_);
    auto it = volume_history_.find(symbol);
    if (it == volume_history_.end() || it->second.avg_volume <= 0) {
        return 1.0;  // No historical data; default volume ratio = 1
    }
    
    // Estimate volume based on elapsed trading minutes
    // HK trading hours: 9:30-12:00 + 13:00-16:00 = 330 minutes
    auto time_pair = getCurrentTime();
    int current_min = time_pair.first * 60 + time_pair.second;
    
    int elapsed_minutes = 0;
    int morning_open = 9 * 60 + 30;
    int morning_close = 12 * 60;
    int afternoon_open = 13 * 60;
    
    if (current_min <= morning_close) {
        elapsed_minutes = std::max(1, current_min - morning_open);
    } else if (current_min < afternoon_open) {
        elapsed_minutes = morning_close - morning_open;  // 150 minutes
    } else {
        elapsed_minutes = (morning_close - morning_open) + std::max(1, current_min - afternoon_open);
    }
    
    // Extrapolate to estimate full-day volume based on elapsed minutes
    double total_trading_minutes = 330.0;
    double estimated_daily_volume = (double)current_volume * total_trading_minutes / elapsed_minutes;
    
    return estimated_daily_volume / it->second.avg_volume;
}

double MarketScanner::calculateSpeed(const std::string& symbol, double current_price) const {
    std::lock_guard<std::mutex> lock(last_snapshots_mutex_);
    
    auto it = last_snapshots_.find(symbol);
    if (it == last_snapshots_.end() || it->second.last_price <= 0) {
        return 0.0;
    }
    
    // Speed = (current_price - last_price) / last_price
    return (current_price - it->second.last_price) / it->second.last_price;
}

double MarketScanner::calculateBidAskRatio(const Snapshot& snapshot) const {
    if (snapshot.ask_volume_1 <= 0) {
        return (snapshot.bid_volume_1 > 0) ? 10.0 : 1.0;
    }
    return (double)snapshot.bid_volume_1 / snapshot.ask_volume_1;
}

void MarketScanner::updateVolumeHistory(const std::string& symbol, int64_t volume, double price) {
    std::lock_guard<std::mutex> lock(volume_history_mutex_);
    auto& history = volume_history_[symbol];
    history.last_price = price;
    history.last_scan_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void MarketScanner::initVolumeHistory(
    const std::shared_ptr<IExchange>& exchange, 
    const std::vector<std::string>& symbols) {
    
    if (!exchange || !exchange->isConnected()) return;
    
    LOG_INFO("Initializing volume history for breakout detection (" + 
             std::to_string(symbols.size()) + " symbols)...");
    
    int count = 0;
    
    for (const auto& symbol : symbols) {
        if (!running_) break;
        
        try {
            // Fetch recent N days of daily K-line data
            auto klines = exchange->getHistoryKLine(symbol, "K_DAY", VOLUME_HISTORY_DAYS + 1);
            
            if (klines.size() >= 2) {
                std::lock_guard<std::mutex> lock(volume_history_mutex_);
                auto& history = volume_history_[symbol];
                history.daily_volumes.clear();
                
                // Skip today's data (last entry), use previous historical entries
                for (size_t j = 0; j < klines.size() - 1; ++j) {
                    history.daily_volumes.push_back(klines[j].volume);
                }
                
                // Compute average volume
                if (!history.daily_volumes.empty()) {
                    int64_t total = 0;
                    for (auto v : history.daily_volumes) total += v;
                    history.avg_volume = total / (int64_t)history.daily_volumes.size();
                }
                
                history.last_price = klines.back().close_price;
                count++;
            }
        } catch (const std::exception& e) {
            // Silently skip stocks that fail
            (void)e;
        }
        
        if (count % 50 == 0 && count > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
    
    LOG_INFO("Volume history initialized for " + std::to_string(count) + " stocks");
}

