#include "exchange/futu_exchange.h"
#include "event/event_interface.h"
#include "event/event.h"
#include "utils/logger_defines.h"
#include <sstream>
#include <chrono>
#include <ctime>

#ifdef ENABLE_FUTU
#include "futu_spi.h"
using namespace Futu;
#endif

#define CLASS_NAME "futu"


FutuExchange::FutuExchange(IEventEngine* event_engine, const FutuConfig& config)
    : event_engine_(event_engine)
    , config_(config)
    , connected_(false) {
    #ifdef ENABLE_FUTU
    spi_ = nullptr;
    #endif
    
    writeLog(LogLevel::Info, "Futu Exchange initialized");
}

FutuExchange::~FutuExchange() {
    disconnect();
}

// ========== Connection Management ==========

bool FutuExchange::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connected_) {
        writeLog(LogLevel::Warn, "Already connected to Futu API");
        return true;
    }
    
    writeLog(LogLevel::Info, "Connecting to Futu API...");
    
    #ifdef ENABLE_FUTU
    try {
        // Create SPI callback handler and API manager
        spi_ = new FutuSpi(this);
        
        // Initialize API
        if (!spi_->InitApi(config_.host, config_.port)) {
            writeLog(LogLevel::Error, "Failed to initialize FTAPI");
            delete spi_;
            spi_ = nullptr;
            return false;
        }
        
        // Wait for successful connection
        
        writeLog(LogLevel::Info, "FTAPI connection initialized");
        
        // If this is live trading, unlocking is required
        if (!config_.is_simulation && !config_.unlock_password.empty()) {
            if (!unlockTrade()) {
                writeLog(LogLevel::Error, "Failed to unlock trade");
                disconnect();
                return false;
            }
        }
        
        // Retrieve account list
        if (!getAccountList()) {
            writeLog(LogLevel::Warn, "Failed to get account list");
        }
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during FTAPI connection: ") + e.what());
        return false;
    }
    #else
    writeLog(LogLevel::Warn, "FTAPI is not enabled, running in simulation mode");
    #endif
    
    connected_ = true;
    
    std::stringstream ss;
    ss << "Connected to Futu API at " << config_.host << ":" << config_.port;
    if (config_.is_simulation) {
        ss << " (Simulation Mode)";
    }
    writeLog(LogLevel::Info, ss.str());
    
    return true;
}

bool FutuExchange::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        return true;
    }
    
    #ifdef ENABLE_FUTU
    if (spi_ != nullptr) {
        spi_->ReleaseApi();
        delete spi_;
        spi_ = nullptr;
    }
    #endif
    
    connected_ = false;
    writeLog(LogLevel::Info, "Disconnected from Futu API");
    
    return true;
}

bool FutuExchange::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connected_;
}

// ========== Internal helper methods ==========

bool FutuExchange::unlockTrade() {
    if (config_.unlock_password.empty()) {
        writeLog(LogLevel::Error, "Unlock password is empty for real trading");
        return false;
    }
    
    #ifdef ENABLE_FUTU
    if (spi_ == nullptr) {
        writeLog(LogLevel::Error, "SPI not initialized");
        return false;
    }
    
    try {
        Futu::u32_t serial_no = spi_->SendUnlockTrade(config_.unlock_password);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send unlock trade request");
            return false;
        }
        
        // wait for reply
        if (!spi_->WaitForReply(serial_no, 5000)) {
            writeLog(LogLevel::Error, "Unlock trade timeout");
            return false;
        }
        
        writeLog(LogLevel::Info, "Trade unlocked successfully");
        return true;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during unlock trade: ") + e.what());
        return false;
    }
    #else
    writeLog(LogLevel::Warn, "FTAPI is not enabled");
    return false;
    #endif
}

bool FutuExchange::getAccountList() {
    #ifdef ENABLE_FUTU
    if (spi_ == nullptr) {
        writeLog(LogLevel::Error, "SPI not initialized");
        return false;
    }
    
    try {
        Futu::u32_t serial_no = spi_->SendGetAccList();
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send get account list request");
            return false;
        }
        
        // wait for reply
        if (!spi_->WaitForReply(serial_no, 5000)) {
            writeLog(LogLevel::Error, "Get account list timeout");
            return false;
        }
        
        // extract account list from response
        {
            std::lock_guard<std::mutex> lock(spi_->mutex_);
            auto it = spi_->acc_list_responses_.find(serial_no);
            if (it != spi_->acc_list_responses_.end()) {
                const auto& rsp = it->second;
                if (rsp.rettype() >= 0 && rsp.has_s2c()) {
                    const auto& s2c = rsp.s2c();
                    int acc_count = s2c.acclist_size();
                    
                    writeLog(LogLevel::Info, std::string("Found ") + std::to_string(acc_count) + " accounts");
                    
                    for (int i = 0; i < acc_count; ++i) {
                        const auto& acc = s2c.acclist(i);
                        account_ids_.push_back(acc.accid());
                        
                        std::stringstream ss;
                        ss << "Account " << i << ": ID=" << acc.accid() 
                           << ", TrdEnv=" << acc.trdenv()
                           << ", TrdMarket=" << acc.trdmarketauthlist_size();
                        writeLog(LogLevel::Info, ss.str());
                    }
                }
                spi_->acc_list_responses_.erase(it);
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during get account list: ") + e.what());
        return false;
    }
    #else
    writeLog(LogLevel::Warn, "FTAPI is not enabled");
    return false;
    #endif
}

Qot_Common::Security FutuExchange::convertToSecurity(const std::string& symbol) {
    Qot_Common::Security security;
    
    #ifdef ENABLE_FUTU
    // Parse symbol; format may be "00700" or "HK.00700"
    std::string market_str = config_.market;
    std::string code = symbol;
    
    size_t dot_pos = symbol.find('.');
    if (dot_pos != std::string::npos) {
        market_str = symbol.substr(0, dot_pos);
        code = symbol.substr(dot_pos + 1);
    }
    
    // Convert market type
    int32_t market_type = Qot_Common::QotMarket_HK_Security;
    if (market_str == "HK") {
        market_type = Qot_Common::QotMarket_HK_Security;
    } else if (market_str == "US") {
        market_type = Qot_Common::QotMarket_US_Security;
    } else if (market_str == "SH") {
        market_type = Qot_Common::QotMarket_CNSH_Security;
    } else if (market_str == "SZ") {
        market_type = Qot_Common::QotMarket_CNSZ_Security;
    }
    
    security.set_market(market_type);
    security.set_code(code);
    #endif
    
    return security;
}

int32_t FutuExchange::convertKLineType(const std::string& kline_type) {
    #ifdef ENABLE_FUTU
    if (kline_type == "1m" || kline_type == "1min" || kline_type == "K_1M") {
        return Qot_Common::KLType_1Min;
    } else if (kline_type == "3m" || kline_type == "3min" || kline_type == "K_3M") {
        return Qot_Common::KLType_3Min;
    } else if (kline_type == "5m" || kline_type == "5min" || kline_type == "K_5M") {
        return Qot_Common::KLType_5Min;
    } else if (kline_type == "15m" || kline_type == "15min" || kline_type == "K_15M") {
        return Qot_Common::KLType_15Min;
    } else if (kline_type == "30m" || kline_type == "30min" || kline_type == "K_30M") {
        return Qot_Common::KLType_30Min;
    } else if (kline_type == "60m" || kline_type == "60min" || kline_type == "1h" || kline_type == "K_60M") {
        return Qot_Common::KLType_60Min;
    } else if (kline_type == "1d" || kline_type == "day" || kline_type == "K_DAY") {
        return Qot_Common::KLType_Day;
    } else if (kline_type == "1w" || kline_type == "week" || kline_type == "K_WEEK") {
        return Qot_Common::KLType_Week;
    } else if (kline_type == "1mon" || kline_type == "month" || kline_type == "K_MON") {
        return Qot_Common::KLType_Month;
    }
    
    // Default to 5 minutes
    return Qot_Common::KLType_5Min;
    #else
    return 0;
    #endif
}

// ========== Account Related ==========

AccountInfo FutuExchange::getAccountInfo() {
    if (!connected_) {
        writeLog(LogLevel::Error, "Not connected to exchange");
        return AccountInfo();
    }
    
    AccountInfo info;
    
    #ifdef ENABLE_FUTU
    if (spi_ == nullptr) {
        writeLog(LogLevel::Error, "SPI not initialized");
        return info;
    }
    
    if (account_ids_.empty()) {
        writeLog(LogLevel::Error, "No account available");
        return info;
    }
    
    try {
        // use the first account
        uint64_t acc_id = account_ids_[0];
        
        Futu::u32_t serial_no = spi_->SendGetFunds(acc_id, 
            config_.is_simulation ? Trd_Common::TrdEnv_Simulate : Trd_Common::TrdEnv_Real,
            Trd_Common::TrdMarket_HK);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send get funds request");
            return info;
        }
        
        // wait for reply
        if (!spi_->WaitForReply(serial_no, 5000)) {
            writeLog(LogLevel::Error, "Get funds timeout");
            return info;
        }
        
        // extract funds info from response
        {
            std::lock_guard<std::mutex> lock(spi_->mutex_);
            auto it = spi_->funds_responses_.find(serial_no);
            if (it != spi_->funds_responses_.end()) {
                const auto& rsp = it->second;
                if (rsp.rettype() >= 0 && rsp.has_s2c()) {
                    const auto& s2c = rsp.s2c();
                    const auto& funds = s2c.funds();
                    
                    info.account_id = std::to_string(acc_id);
                    info.total_assets = funds.totalassets();
                    info.cash = funds.cash();
                    info.market_value = funds.marketval();
                    info.available_funds = funds.availablefunds();
                    info.frozen_funds = funds.frozencash();
                    info.currency = funds.currency();
                }
                spi_->funds_responses_.erase(it);
            }
        }
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during get account info: ") + e.what());
    }
    #endif
    
    writeLog(LogLevel::Info, "Get account info");
    return info;
}

std::vector<ExchangePosition> FutuExchange::getPositions() {
    if (!connected_) {
        writeLog(LogLevel::Error, "Not connected to exchange");
        return {};
    }
    
    std::vector<ExchangePosition> positions;
    
    #ifdef ENABLE_FUTU
    if (spi_ == nullptr) {
        writeLog(LogLevel::Error, "Trd API not initialized");
        return positions;
    }
    
    if (account_ids_.empty()) {
        writeLog(LogLevel::Error, "No account available");
        return positions;
    }
    
    try {
        // use the first account
        uint64_t acc_id = account_ids_[0];
        
        Futu::u32_t serial_no = spi_->SendGetPositionList(acc_id,
            config_.is_simulation ? Trd_Common::TrdEnv_Simulate : Trd_Common::TrdEnv_Real,
            Trd_Common::TrdMarket_HK);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send get position list request");
            return positions;
        }
        
        // wait for reply
        if (!spi_->WaitForReply(serial_no, 5000)) {
            writeLog(LogLevel::Error, "Get position list timeout");
            return positions;
        }
        
        // extract positions info from response
        {
            std::lock_guard<std::mutex> lock(spi_->mutex_);
            auto it = spi_->position_responses_.find(serial_no);
            if (it != spi_->position_responses_.end()) {
                const auto& rsp = it->second;
                if (rsp.rettype() >= 0 && rsp.has_s2c()) {
                    const auto& s2c = rsp.s2c();
                    int pos_count = s2c.positionlist_size();
                    
                    for (int i = 0; i < pos_count; ++i) {
                        const auto& pos = s2c.positionlist(i);
                        
                        ExchangePosition position;
                        position.symbol = pos.code();
                        position.stock_name = pos.name();
                        position.quantity = static_cast<int>(pos.qty());
                        position.avg_price = pos.costprice();
                        position.current_price = pos.price();
                        position.market_value = pos.price() * pos.qty();  // calculate market value
                        position.cost_price = pos.costprice() * pos.qty();
                        position.profit_loss = pos.has_plval() ? pos.plval() : 0.0;
                        position.profit_loss_ratio = pos.has_plratio() ? pos.plratio() : 0.0;
                        
                        positions.push_back(position);
                    }
                }
                spi_->position_responses_.erase(it);
            }
        }
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during get positions: ") + e.what());
    }
    #endif
    
    writeLog(LogLevel::Info, std::string("Queried ") + std::to_string(positions.size()) + " positions");
    return positions;
}

double FutuExchange::getAvailableFunds() {
    AccountInfo info = getAccountInfo();
    return info.available_funds;
}

// ========== Trading Related ==========

std::string FutuExchange::placeOrder(
    const std::string& symbol,
    const std::string& side,
    int quantity,
    const std::string& order_type,
    double price) {
    
    if (!connected_) {
        writeLog(LogLevel::Error, "Not connected to exchange");
        return "";
    }
    
    std::stringstream ss;
    ss << "Placing order: " << symbol << " " << side << " " << quantity;
    if (order_type == "LIMIT") {
        ss << " @ " << price;
    }
    writeLog(LogLevel::Info, ss.str());
    
    std::string order_id = "";
    
    #ifdef ENABLE_FUTU
    if (spi_ == nullptr) {
        writeLog(LogLevel::Error, "SPI not initialized");
        return order_id;
    }
    
    if (account_ids_.empty()) {
        writeLog(LogLevel::Error, "No account available");
        return order_id;
    }
    
    try {
        // Use the first account
        uint64_t acc_id = account_ids_[0];
        
        // Set trade side
        int order_side = Trd_Common::TrdSide_Buy;
        if (side == "BUY") {
            order_side = Trd_Common::TrdSide_Buy;
        } else {
            order_side = Trd_Common::TrdSide_Sell;
        }
        
        // Set order type
        int order_type_val = Trd_Common::OrderType_Normal;
        if (order_type == "MARKET") {
            order_type_val = Trd_Common::OrderType_Market;
            price = 0.0;  // Market orders do not need a price
        }
        
        // Create security object
        Qot_Common::Security security = convertToSecurity(symbol);
        
        Futu::u32_t serial_no = spi_->SendPlaceOrder(acc_id,
            config_.is_simulation ? Trd_Common::TrdEnv_Simulate : Trd_Common::TrdEnv_Real,
            Trd_Common::TrdMarket_HK,
            security, order_side, order_type_val, quantity, price);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send place order request");
            return order_id;
        }
        
        // Wait for response
        if (!spi_->WaitForReply(serial_no, 5000)) {
            writeLog(LogLevel::Error, "Place order timeout");
            return order_id;
        }
        
        // Extract order ID from response
        {
            std::lock_guard<std::mutex> lock(spi_->mutex_);
            auto it = spi_->place_order_responses_.find(serial_no);
            if (it != spi_->place_order_responses_.end()) {
                const auto& rsp = it->second;
                if (rsp.rettype() >= 0 && rsp.has_s2c()) {
                    order_id = std::to_string(rsp.s2c().orderid());
                    writeLog(LogLevel::Info, std::string("Order placed successfully: ") + order_id);
                } else {
                    writeLog(LogLevel::Error, std::string("Place order failed: ") + rsp.retmsg());
                }
                spi_->place_order_responses_.erase(it);
            }
        }
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during place order: ") + e.what());
    }
    #endif
    
    return order_id;
}

bool FutuExchange::cancelOrder(const std::string& order_id) {
    if (!connected_) {
        writeLog(LogLevel::Error, "Not connected to exchange");
        return false;
    }
    
    #ifdef ENABLE_FUTU
    if (spi_ == nullptr) {
        writeLog(LogLevel::Error, "SPI not initialized");
        return false;
    }
    
    if (account_ids_.empty()) {
        writeLog(LogLevel::Error, "No account available");
        return false;
    }
    
    try {
        uint64_t acc_id = account_ids_[0];
        uint64_t order_id_num = std::stoull(order_id);
        
        Futu::u32_t serial_no = spi_->SendCancelOrder(acc_id,
            config_.is_simulation ? Trd_Common::TrdEnv_Simulate : Trd_Common::TrdEnv_Real,
            order_id_num);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send cancel order request");
            return false;
        }
        
        // Wait for response
        if (!spi_->WaitForReply(serial_no, 5000)) {
            writeLog(LogLevel::Error, "Cancel order timeout");
            return false;
        }
        
        writeLog(LogLevel::Info, std::string("Order cancelled: ") + order_id);
        return true;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during cancel order: ") + e.what());
        return false;
    }
    #else
    writeLog(LogLevel::Info, std::string("Order cancelled (simulation): ") + order_id);
    return true;
    #endif
}

bool FutuExchange::modifyOrder(const std::string& order_id, int new_quantity, double new_price) {
    if (!connected_) {
        writeLog(LogLevel::Error, "Not connected to exchange");
        return false;
    }
    
    #ifdef ENABLE_FUTU
    if (spi_ == nullptr) {
        writeLog(LogLevel::Error, "SPI not initialized");
        return false;
    }
    
    if (account_ids_.empty()) {
        writeLog(LogLevel::Error, "No account available");
        return false;
    }
    
    try {
        uint64_t acc_id = account_ids_[0];
        uint64_t order_id_num = std::stoull(order_id);
        
        Futu::u32_t serial_no = spi_->SendModifyOrder(acc_id,
            config_.is_simulation ? Trd_Common::TrdEnv_Simulate : Trd_Common::TrdEnv_Real,
            order_id_num, new_quantity, new_price);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send modify order request");
            return false;
        }
        
        // Wait for response
        if (!spi_->WaitForReply(serial_no, 5000)) {
            writeLog(LogLevel::Error, "Modify order timeout");
            return false;
        }
        
        std::stringstream ss;
        ss << "Order modified: " << order_id << " new_qty=" << new_quantity << " new_price=" << new_price;
        writeLog(LogLevel::Info, ss.str());
        return true;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during modify order: ") + e.what());
        return false;
    }
    #else
    std::stringstream ss;
    ss << "Order modified (simulation): " << order_id << " new_qty=" << new_quantity << " new_price=" << new_price;
    writeLog(LogLevel::Info, ss.str());
    return true;
    #endif
}

OrderData FutuExchange::getOrderStatus(const std::string& order_id) {
    OrderData order;
    order.order_id = order_id;
    order.status = OrderStatus::SUBMITTED;
    
    #ifdef ENABLE_FUTU
    // TODO: implement order status query
    // You can use the GetOrderList API to query today's orders
    #endif
    
    return order;
}

std::vector<OrderData> FutuExchange::getOrderHistory(int days) {
    (void)days;
    
    #ifdef ENABLE_FUTU
    // TODO: implement historical order query
    // Use the GetHistoryOrderList API
    #endif
    
    return {};
}

// ========== Market Data Related ==========

bool FutuExchange::subscribeKLine(const std::string& symbol, const std::string& kline_type) {
    if (!connected_) {
        writeLog(LogLevel::Error, "Not connected to exchange");
        return false;
    }
    
    #ifdef ENABLE_FUTU
    if (spi_ == nullptr) {
        writeLog(LogLevel::Error, "SPI not initialized");
        return false;
    }
    
    try {
        // create security object
        Qot_Common::Security security = convertToSecurity(symbol);
        int32_t kl_type = convertKLineType(kline_type);
        
        Futu::u32_t serial_no = spi_->SendSubscribeKLine(security, kl_type);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send subscribe KLine request");
            return false;
        }
        
        // wait for reply
        if (!spi_->WaitForReply(serial_no, 5000)) {
            writeLog(LogLevel::Error, "Subscribe KLine timeout");
            return false;
        }
        
        std::stringstream ss;
        ss << "Subscribed KLine: " << symbol << " " << kline_type;
        writeLog(LogLevel::Info, ss.str());
        return true;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during subscribe KLine: ") + e.what());
        return false;
    }
    #else
    std::stringstream ss;
    ss << "Subscribed KLine (simulation): " << symbol << " " << kline_type;
    writeLog(LogLevel::Info, ss.str());
    return true;
    #endif
}

bool FutuExchange::unsubscribeKLine(const std::string& symbol) {
    if (!connected_) {
        return false;
    }
    
    #ifdef ENABLE_FUTU
    if (spi_ == nullptr) {
        return false;
    }
    
    try {
        Qot_Common::Security security = convertToSecurity(symbol);
        
        Futu::u32_t serial_no = spi_->SendUnsubscribeKLine(security);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send unsubscribe KLine request");
            return false;
        }
        
        std::stringstream ss;
        ss << "Unsubscribed KLine: " << symbol;
        writeLog(LogLevel::Info, ss.str());
        return true;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during unsubscribe KLine: ") + e.what());
        return false;
    }
    #else
    std::stringstream ss;
    ss << "Unsubscribed KLine (simulation): " << symbol;
    writeLog(LogLevel::Info, ss.str());
    return true;
    #endif
}

bool FutuExchange::subscribeTick(const std::string& symbol) {
    if (!connected_) {
        writeLog(LogLevel::Error, "Not connected to exchange");
        return false;
    }
    
    #ifdef ENABLE_FUTU
    if (spi_ == nullptr) {
        writeLog(LogLevel::Error, "SPI not initialized");
        return false;
    }
    
    try {
        Qot_Common::Security security = convertToSecurity(symbol);
        
        Futu::u32_t serial_no = spi_->SendSubscribeTick(security);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send subscribe Tick request");
            return false;
        }
        
        // wait for reply
        if (!spi_->WaitForReply(serial_no, 5000)) {
            writeLog(LogLevel::Error, "Subscribe Tick timeout");
            return false;
        }
        
        std::stringstream ss;
        ss << "Subscribed Tick: " << symbol;
        writeLog(LogLevel::Info, ss.str());
        return true;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during subscribe Tick: ") + e.what());
        return false;
    }
    #else
    std::stringstream ss;
    ss << "Subscribed Tick (simulation): " << symbol;
    writeLog(LogLevel::Info, ss.str());
    return true;
    #endif
}

bool FutuExchange::unsubscribeTick(const std::string& symbol) {
    (void)symbol;
    
    if (!connected_) {
        return false;
    }
    
    // TODO: implement unsubscribe Tick
    return true;
}

std::vector<KlineData> FutuExchange::getHistoryKLine(
    const std::string& symbol,
    const std::string& kline_type,
    int count) {
    
    if (!connected_) {
        writeLog(LogLevel::Error, "Not connected to exchange");
        return {};
    }
    
    std::vector<KlineData> klines;
    
    #ifdef ENABLE_FUTU
    if (spi_ == nullptr) {
        writeLog(LogLevel::Error, "SPI not initialized");
        return klines;
    }
    
    try {
        Qot_Common::Security security = convertToSecurity(symbol);
        int32_t kl_type = convertKLineType(kline_type);
        
        Futu::u32_t serial_no = spi_->SendGetHistoryKLine(security, kl_type, count);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send get history KLine request");
            return klines;
        }
        
        // Wait for response
        if (!spi_->WaitForReply(serial_no, 10000)) {
            writeLog(LogLevel::Error, "Get history KLine timeout");
            return klines;
        }
        
        // Extract K-line data from response
        {
            std::lock_guard<std::mutex> lock(spi_->mutex_);
            auto it = spi_->history_kline_responses_.find(serial_no);
            if (it != spi_->history_kline_responses_.end()) {
                const auto& rsp = it->second;
                if (rsp.rettype() >= 0 && rsp.has_s2c()) {
                    const auto& s2c = rsp.s2c();
                    int kl_count = s2c.kllist_size();
                    
                    for (int i = 0; i < kl_count; ++i) {
                        const auto& kl = s2c.kllist(i);
                        
                        KlineData kline;
                        kline.symbol = symbol;
                        kline.exchange = getName();
                        kline.datetime = kl.time();
                        kline.interval = kline_type;
                        kline.open_price = kl.openprice();
                        kline.high_price = kl.highprice();
                        kline.low_price = kl.lowprice();
                        kline.close_price = kl.closeprice();
                        kline.volume = kl.volume();
                        kline.turnover = kl.turnover();
                        
                        klines.push_back(kline);
                    }
                }
                spi_->history_kline_responses_.erase(it);
            }
        }
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during get history KLine: ") + e.what());
    }
    #endif
    
    writeLog(LogLevel::Info, std::string("Got ") + std::to_string(klines.size()) + " history KLines");
    return klines;
}

Snapshot FutuExchange::getSnapshot(const std::string& symbol) {
    if (!connected_) {
        writeLog(LogLevel::Error, "Not connected to exchange");
        return Snapshot();
    }
    
    Snapshot snapshot;
    
    #ifdef ENABLE_FUTU
    if (spi_ == nullptr) {
        writeLog(LogLevel::Error, "SPI not initialized");
        return snapshot;
    }
    
    try {
        Qot_Common::Security security = convertToSecurity(symbol);
        std::vector<Qot_Common::Security> securities = {security};
        
        Futu::u32_t serial_no = spi_->SendGetSecuritySnapshot(securities);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send get snapshot request");
            return snapshot;
        }
        
        // Wait for response
        if (!spi_->WaitForReply(serial_no, 5000)) {
            writeLog(LogLevel::Error, "Get snapshot timeout");
            return snapshot;
        }
        
        // Extract snapshot data from response
        {
            std::lock_guard<std::mutex> lock(spi_->mutex_);
            auto it = spi_->snapshot_responses_.find(serial_no);
            if (it != spi_->snapshot_responses_.end()) {
                const auto& rsp = it->second;
                if (rsp.rettype() >= 0 && rsp.has_s2c()) {
                    const auto& s2c = rsp.s2c();
                    if (s2c.snapshotlist_size() > 0) {
                        const auto& snap = s2c.snapshotlist(0);
                        const auto& basic = snap.basic();
                        
                        snapshot.symbol = symbol;
                        snapshot.name = basic.name();
                        snapshot.exchange = getName();
                        snapshot.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
                        snapshot.last_price = basic.curprice();
                        snapshot.open_price = basic.openprice();
                        snapshot.high_price = basic.highprice();
                        snapshot.low_price = basic.lowprice();
                        snapshot.pre_close = basic.lastcloseprice();
                        snapshot.volume = basic.volume();
                        snapshot.turnover = basic.turnover();
                        snapshot.turnover_rate = basic.turnoverrate();
                        snapshot.price_change = basic.has_amplitude() ? basic.amplitude() : 0.0;
                        snapshot.price_change_abs = 0.0;  // Futu API does not provide absolute change value
                        snapshot.ask_price_1 = basic.has_askprice() ? basic.askprice() : 0.0;
                        snapshot.bid_price_1 = basic.has_bidprice() ? basic.bidprice() : 0.0;
                        snapshot.ask_volume_1 = basic.has_askvol() ? basic.askvol() : 0.0;
                        snapshot.bid_volume_1 = basic.has_bidvol() ? basic.bidvol() : 0.0;
                    }
                }
                spi_->snapshot_responses_.erase(it);
            }
        }
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during get snapshot: ") + e.what());
    }
    #endif
    
    return snapshot;
}

// ========== Market scanning ==========

std::vector<std::string> FutuExchange::getMarketStockList() {
    if (!connected_) {
        writeLog(LogLevel::Error, "Not connected to exchange");
        return {};
    }
    
    std::vector<std::string> stocks;
    
    #ifdef ENABLE_FUTU
    if (spi_ == nullptr) {
        writeLog(LogLevel::Error, "SPI not initialized");
        return stocks;
    }
    
    try {
        // Determine market type, corresponds to Python 'market' parameter
        int32_t market_type = Qot_Common::QotMarket_HK_Security;
        if (config_.market == "HK") {
            market_type = Qot_Common::QotMarket_HK_Security;
        } else if (config_.market == "US") {
            market_type = Qot_Common::QotMarket_US_Security;
        } else if (config_.market == "SH") {
            market_type = Qot_Common::QotMarket_CNSH_Security;
        } else if (config_.market == "SZ") {
            market_type = Qot_Common::QotMarket_CNSZ_Security;
        }
        
        std::stringstream ss;
        ss << "Getting stock basic info for market: " << config_.market;
        writeLog(LogLevel::Info, ss.str());
        
        // Use GetStaticInfo API to retrieve all stocks in the market (equivalent to Python's get_stock_basicinfo)
        // Pass market type and security type (SecurityType_Eqty indicates equities)
        Futu::u32_t serial_no = spi_->SendGetStaticInfo(market_type, Qot_Common::SecurityType_Eqty);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send get static info request");
            return stocks;
        }
        
        // Wait for response (15s timeout because stock list may be large)
        if (!spi_->WaitForReply(serial_no, 15000)) {
            writeLog(LogLevel::Error, "Get static info timeout");
            return stocks;
        }
        
        // Extract stock codes from response
        {
            std::lock_guard<std::mutex> lock(spi_->mutex_);
            auto it = spi_->static_info_responses_.find(serial_no);
            if (it != spi_->static_info_responses_.end()) {
                const auto& rsp = it->second;
                if (rsp.rettype() >= 0 && rsp.has_s2c()) {
                    const auto& s2c = rsp.s2c();
                    int static_info_count = s2c.staticinfolist_size();
                    
                    std::stringstream log_ss;
                    log_ss << "Found " << static_info_count << " stocks in market " << config_.market;
                    writeLog(LogLevel::Info, log_ss.str());
                    
                    for (int i = 0; i < static_info_count; ++i) {
                        const auto& info = s2c.staticinfolist(i);
                        if (info.has_basic() && info.basic().has_security()) {
                            const auto& sec = info.basic().security();
                            std::string code = sec.code();
                            if (!code.empty()) {
                                stocks.push_back(code);
                            }
                        }
                    }
                } else {
                    writeLog(LogLevel::Error, std::string("Get static info failed: ") + rsp.retmsg());
                }
                spi_->static_info_responses_.erase(it);
            }
        }
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during get market stock list: ") + e.what());
    }
    #else
    LOG_WARN("FTAPI is not enabled, returning sample stocks");
    stocks.push_back("00700");  // Tencent
    stocks.push_back("09988");  // Alibaba
    stocks.push_back("03690");  // Meituan
    stocks.push_back("01810");  // Xiaomi
    stocks.push_back("02318");  // Ping An
    #endif
    
    std::stringstream ss;
    ss << "Retrieved " << stocks.size() << " stocks from market " << config_.market;
    writeLog(LogLevel::Info, ss.str());
    
    return stocks;
}

std::map<std::string, Snapshot> FutuExchange::getBatchSnapshots(
    const std::vector<std::string>& stock_codes) {
    
    if (!connected_) {
        writeLog(LogLevel::Error, "Not connected to exchange");
        return {};
    }
    
    std::map<std::string, Snapshot> snapshots;
    
    #ifdef ENABLE_FUTU
    if (spi_ == nullptr) {
        writeLog(LogLevel::Error, "SPI not initialized");
        return snapshots;
    }
    
    try {
        // Convert stock code list
        std::vector<Qot_Common::Security> securities;
        for (const auto& code : stock_codes) {
            securities.push_back(convertToSecurity(code));
        }
        
        Futu::u32_t serial_no = spi_->SendGetSecuritySnapshot(securities);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send batch get snapshot request");
            return snapshots;
        }
        
        // Wait for response
        if (!spi_->WaitForReply(serial_no, 10000)) {
            writeLog(LogLevel::Error, "Batch get snapshot timeout");
            return snapshots;
        }
        
        // Extract snapshot data from response
        {
            std::lock_guard<std::mutex> lock(spi_->mutex_);
            auto it = spi_->snapshot_responses_.find(serial_no);
            if (it != spi_->snapshot_responses_.end()) {
                const auto& rsp = it->second;
                if (rsp.rettype() >= 0 && rsp.has_s2c()) {
                    const auto& s2c = rsp.s2c();
                    int snap_count = s2c.snapshotlist_size();
                    
                    for (int i = 0; i < snap_count; ++i) {
                        const auto& snap = s2c.snapshotlist(i);
                        const auto& basic = snap.basic();
                        const auto& sec = basic.security();
                        
                        Snapshot snapshot;
                        snapshot.symbol = sec.code();
                        snapshot.name = basic.name();
                        snapshot.exchange = getName();
                        snapshot.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
                        snapshot.last_price = basic.curprice();
                        snapshot.open_price = basic.openprice();
                        snapshot.high_price = basic.highprice();
                        snapshot.low_price = basic.lowprice();
                        snapshot.pre_close = basic.lastcloseprice();
                        snapshot.volume = basic.volume();
                        snapshot.turnover = basic.turnover();
                        snapshot.turnover_rate = basic.turnoverrate();
                        snapshot.price_change = basic.has_amplitude() ? basic.amplitude() : 0.0;
                        snapshot.price_change_abs = 0.0;  // Futu API does not provide absolute change value
                        snapshot.ask_price_1 = basic.has_askprice() ? basic.askprice() : 0.0;
                        snapshot.bid_price_1 = basic.has_bidprice() ? basic.bidprice() : 0.0;
                        snapshot.ask_volume_1 = basic.has_askvol() ? basic.askvol() : 0.0;
                        snapshot.bid_volume_1 = basic.has_bidvol() ? basic.bidvol() : 0.0;

                        snapshots[sec.code()] = snapshot;
                    }
                }
                spi_->snapshot_responses_.erase(it);
            }
        }
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during batch get snapshots: ") + e.what());
    }
    #endif
    
    writeLog(LogLevel::Info, std::string("Got ") + std::to_string(snapshots.size()) + " snapshots");
    return snapshots;
}


// ========== Event engine ==========
void FutuExchange::writeLog(LogLevel level, const std::string& message) {
    auto current_timestamp =  std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    if (event_engine_) {
        // Publish logs via event engine
        LogData log_data;
        log_data.level = level;
        log_data.message = "[FutuExchange] " + message;
        log_data.timestamp = current_timestamp;
        
        auto event = std::make_shared<Event>(EventType::EVENT_LOG);
        event->setData(log_data);
        event_engine_->putEvent(event);
    } else {

        auto strLevel = levelToString(level);

        // Fallback to direct Logger
        switch (level) {
            case LogLevel::Debug:
            case LogLevel::Info:
            case LogLevel::Warn:
                std::cout << current_timestamp << strLevel << message << std::endl;
                break;
            case LogLevel::Error:
                 std::cerr << current_timestamp << strLevel << message << std::endl;
                break;
        }
    }
}

const char* GetExchangeClass() {
    return CLASS_NAME;
}

IExchange* GetExchangeInstance(IEventEngine* event_engine, const std::map<std::string, std::string>& config) {

    FutuConfig futu_config;
    
    // Read params from config
    if (config.find("host") != config.end()) {
        futu_config.host = config.at("host");
    }
    if (config.find("port") != config.end()) {
        futu_config.port = std::stoi(config.at("port"));
    }
    if (config.find("unlock_password") != config.end()) {
        futu_config.unlock_password = config.at("unlock_password");
    }
    if (config.find("is_simulation") != config.end()) {
        futu_config.is_simulation = (config.at("is_simulation") == "true" || config.at("is_simulation") == "1");
    }
    if (config.find("market") != config.end()) {
        futu_config.market = config.at("market");
    }
    
    return new FutuExchange(event_engine, futu_config);
}