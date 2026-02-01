#include "exchange/futu_exchange.h"
#include "utils/logger.h"
#include <sstream>
#include <chrono>
#include <ctime>

#ifdef ENABLE_FUTU
#include "futu_spi.h"
using namespace Futu;
#endif

FutuExchange::FutuExchange(const FutuConfig& config)
    : config_(config), connected_(false) {
    #ifdef ENABLE_FUTU
    qot_api_ = nullptr;
    trd_api_ = nullptr;
    spi_ = nullptr;
    #endif
    
    LOG_INFO("Futu Exchange initialized");
}

FutuExchange::~FutuExchange() {
    disconnect();
}

// ========== 连接管理 ==========

bool FutuExchange::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connected_) {
        LOG_WARNING("Already connected to Futu API");
        return true;
    }
    
    LOG_INFO("Connecting to Futu API...");
    
    #ifdef ENABLE_FUTU
    try {
        // 初始化 FTAPI
        FTAPI::Init();
        
        // 创建 SPI 回调处理
        spi_ = new FutuSpi(this);
        
        // 创建行情API
        qot_api_ = FTAPI::CreateQotApi();
        if (qot_api_ == nullptr) {
            LOG_ERROR("Failed to create Qot API");
            return false;
        }
        
        // 设置客户端信息
        qot_api_->SetClientInfo("QUANT_TRADING_SYSTEM", 1);
        
        // 注册回调
        qot_api_->RegisterConnSpi(spi_);
        qot_api_->RegisterQotSpi(spi_);
        
        // 初始化连接
        bool ret = qot_api_->InitConnect(config_.host.c_str(), config_.port, false);
        if (!ret) {
            LOG_ERROR("Failed to initialize Qot API connection");
            FTAPI::ReleaseQotApi(qot_api_);
            qot_api_ = nullptr;
            delete spi_;
            spi_ = nullptr;
            return false;
        }
        
        // 等待连接成功
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // 创建交易API
        trd_api_ = FTAPI::CreateTrdApi();
        if (trd_api_ == nullptr) {
            LOG_ERROR("Failed to create Trd API");
            qot_api_->Close();
            FTAPI::ReleaseQotApi(qot_api_);
            qot_api_ = nullptr;
            delete spi_;
            spi_ = nullptr;
            return false;
        }
        
        // 设置客户端信息
        trd_api_->SetClientInfo("QUANT_TRADING_SYSTEM", 1);
        
        // 注册回调
        trd_api_->RegisterConnSpi(spi_);
        trd_api_->RegisterTrdSpi(spi_);
        
        // 初始化连接
        ret = trd_api_->InitConnect(config_.host.c_str(), config_.port, false);
        if (!ret) {
            LOG_ERROR("Failed to initialize Trd API connection");
            qot_api_->Close();
            FTAPI::ReleaseQotApi(qot_api_);
            FTAPI::ReleaseTrdApi(trd_api_);
            qot_api_ = nullptr;
            trd_api_ = nullptr;
            delete spi_;
            spi_ = nullptr;
            return false;
        }
        
        // 等待连接成功
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        LOG_INFO("FTAPI connection initialized");
        
        // 如果是实盘交易，需要解锁
        if (!config_.is_simulation && !config_.unlock_password.empty()) {
            if (!unlockTrade()) {
                LOG_ERROR("Failed to unlock trade");
                disconnect();
                return false;
            }
        }
        
        // 获取账户列表
        if (!getAccountList()) {
            LOG_WARNING("Failed to get account list");
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during FTAPI connection: ") + e.what());
        return false;
    }
    #else
    LOG_WARNING("FTAPI is not enabled, running in simulation mode");
    #endif
    
    connected_ = true;
    
    std::stringstream ss;
    ss << "Connected to Futu API at " << config_.host << ":" << config_.port;
    if (config_.is_simulation) {
        ss << " (Simulation Mode)";
    }
    LOG_INFO(ss.str());
    
    return true;
}

bool FutuExchange::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        return true;
    }
    
    #ifdef ENABLE_FUTU
    if (qot_api_ != nullptr) {
        qot_api_->UnregisterQotSpi();
        qot_api_->UnregisterConnSpi();
        qot_api_->Close();
        FTAPI::ReleaseQotApi(qot_api_);
        qot_api_ = nullptr;
    }
    
    if (trd_api_ != nullptr) {
        trd_api_->UnregisterTrdSpi();
        trd_api_->UnregisterConnSpi();
        trd_api_->Close();
        FTAPI::ReleaseTrdApi(trd_api_);
        trd_api_ = nullptr;
    }
    
    if (spi_ != nullptr) {
        delete spi_;
        spi_ = nullptr;
    }
    
    FTAPI::UnInit();
    #endif
    
    connected_ = false;
    LOG_INFO("Disconnected from Futu API");
    
    return true;
}

bool FutuExchange::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connected_;
}

// ========== 内部辅助方法 ==========

bool FutuExchange::unlockTrade() {
    if (config_.unlock_password.empty()) {
        LOG_ERROR("Unlock password is empty for real trading");
        return false;
    }
    
    #ifdef ENABLE_FUTU
    if (trd_api_ == nullptr || spi_ == nullptr) {
        LOG_ERROR("Trd API not initialized");
        return false;
    }
    
    try {
        Trd_UnlockTrade::Request req;
        auto* c2s = req.mutable_c2s();
        c2s->set_unlock(true);
        c2s->set_pwdmd5(config_.unlock_password);
        c2s->set_securityfirm(Trd_Common::SecurityFirm_FutuSecurities);
        
        u32_t serial_no = trd_api_->UnlockTrade(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send unlock trade request");
            return false;
        }
        
        // 等待响应
        if (!spi_->WaitForReply(serial_no, 5000)) {
            LOG_ERROR("Unlock trade timeout");
            return false;
        }
        
        LOG_INFO("Trade unlocked successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during unlock trade: ") + e.what());
        return false;
    }
    #else
    LOG_WARNING("FTAPI is not enabled");
    return false;
    #endif
}

bool FutuExchange::getAccountList() {
    #ifdef ENABLE_FUTU
    if (trd_api_ == nullptr || spi_ == nullptr) {
        LOG_ERROR("Trd API not initialized");
        return false;
    }
    
    try {
        Trd_GetAccList::Request req;
        auto* c2s = req.mutable_c2s();
        c2s->set_userid(0);  // 0 表示当前连接对应的用户
        
        u32_t serial_no = trd_api_->GetAccList(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send get account list request");
            return false;
        }
        
        // 等待响应
        if (!spi_->WaitForReply(serial_no, 5000)) {
            LOG_ERROR("Get account list timeout");
            return false;
        }
        
        // 从响应中获取账户列表
        {
            std::lock_guard<std::mutex> lock(spi_->mutex_);
            auto it = spi_->acc_list_responses_.find(serial_no);
            if (it != spi_->acc_list_responses_.end()) {
                const auto& rsp = it->second;
                if (rsp.rettype() >= 0 && rsp.has_s2c()) {
                    const auto& s2c = rsp.s2c();
                    int acc_count = s2c.acclist_size();
                    
                    LOG_INFO(std::string("Found ") + std::to_string(acc_count) + " accounts");
                    
                    for (int i = 0; i < acc_count; ++i) {
                        const auto& acc = s2c.acclist(i);
                        account_ids_.push_back(acc.accid());
                        
                        std::stringstream ss;
                        ss << "Account " << i << ": ID=" << acc.accid() 
                           << ", TrdEnv=" << acc.trdenv()
                           << ", TrdMarket=" << acc.trdmarketauthlist_size();
                        LOG_INFO(ss.str());
                    }
                }
                spi_->acc_list_responses_.erase(it);
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during get account list: ") + e.what());
        return false;
    }
    #else
    LOG_WARNING("FTAPI is not enabled");
    return false;
    #endif
}

Qot_Common::Security FutuExchange::convertToSecurity(const std::string& symbol) {
    Qot_Common::Security security;
    
    #ifdef ENABLE_FUTU
    // 解析 symbol，格式可能是 "00700" 或 "HK.00700"
    std::string market_str = config_.market;
    std::string code = symbol;
    
    size_t dot_pos = symbol.find('.');
    if (dot_pos != std::string::npos) {
        market_str = symbol.substr(0, dot_pos);
        code = symbol.substr(dot_pos + 1);
    }
    
    // 转换市场类型
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
    if (kline_type == "1m" || kline_type == "1min") {
        return Qot_Common::KLType_1Min;
    } else if (kline_type == "3m" || kline_type == "3min") {
        return Qot_Common::KLType_3Min;
    } else if (kline_type == "5m" || kline_type == "5min") {
        return Qot_Common::KLType_5Min;
    } else if (kline_type == "15m" || kline_type == "15min") {
        return Qot_Common::KLType_15Min;
    } else if (kline_type == "30m" || kline_type == "30min") {
        return Qot_Common::KLType_30Min;
    } else if (kline_type == "60m" || kline_type == "60min" || kline_type == "1h") {
        return Qot_Common::KLType_60Min;
    } else if (kline_type == "1d" || kline_type == "day") {
        return Qot_Common::KLType_Day;
    } else if (kline_type == "1w" || kline_type == "week") {
        return Qot_Common::KLType_Week;
    } else if (kline_type == "1mon" || kline_type == "month") {
        return Qot_Common::KLType_Month;
    }
    
    // 默认返回5分钟
    return Qot_Common::KLType_5Min;
    #else
    return 0;
    #endif
}

// ========== 账户相关 ==========

AccountInfo FutuExchange::getAccountInfo() {
    if (!connected_) {
        LOG_ERROR("Not connected to exchange");
        return AccountInfo();
    }
    
    AccountInfo info;
    
    #ifdef ENABLE_FUTU
    if (trd_api_ == nullptr || spi_ == nullptr) {
        LOG_ERROR("Trd API not initialized");
        return info;
    }
    
    if (account_ids_.empty()) {
        LOG_ERROR("No account available");
        return info;
    }
    
    try {
        // 使用第一个账户
        uint64_t acc_id = account_ids_[0];
        
        Trd_GetFunds::Request req;
        auto* c2s = req.mutable_c2s();
        auto* header = c2s->mutable_header();
        header->set_trdenv(config_.is_simulation ? Trd_Common::TrdEnv_Simulate : Trd_Common::TrdEnv_Real);
        header->set_accid(acc_id);
        header->set_trdmarket(Trd_Common::TrdMarket_HK);
        
        u32_t serial_no = trd_api_->GetFunds(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send get funds request");
            return info;
        }
        
        // 等待响应
        if (!spi_->WaitForReply(serial_no, 5000)) {
            LOG_ERROR("Get funds timeout");
            return info;
        }
        
        // 从响应中提取资金信息
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
        LOG_ERROR(std::string("Exception during get account info: ") + e.what());
    }
    #endif
    
    LOG_INFO("Get account info");
    return info;
}

std::vector<ExchangePosition> FutuExchange::getPositions() {
    if (!connected_) {
        LOG_ERROR("Not connected to exchange");
        return {};
    }
    
    std::vector<ExchangePosition> positions;
    
    #ifdef ENABLE_FUTU
    if (trd_api_ == nullptr || spi_ == nullptr) {
        LOG_ERROR("Trd API not initialized");
        return positions;
    }
    
    if (account_ids_.empty()) {
        LOG_ERROR("No account available");
        return positions;
    }
    
    try {
        // 使用第一个账户
        uint64_t acc_id = account_ids_[0];
        
        Trd_GetPositionList::Request req;
        auto* c2s = req.mutable_c2s();
        auto* header = c2s->mutable_header();
        header->set_trdenv(config_.is_simulation ? Trd_Common::TrdEnv_Simulate : Trd_Common::TrdEnv_Real);
        header->set_accid(acc_id);
        header->set_trdmarket(Trd_Common::TrdMarket_HK);
        
        u32_t serial_no = trd_api_->GetPositionList(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send get position list request");
            return positions;
        }
        
        // 等待响应
        if (!spi_->WaitForReply(serial_no, 5000)) {
            LOG_ERROR("Get position list timeout");
            return positions;
        }
        
        // 从响应中提取持仓信息
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
                        position.market_value = pos.price() * pos.qty();  // 计算市值
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
        LOG_ERROR(std::string("Exception during get positions: ") + e.what());
    }
    #endif
    
    LOG_INFO(std::string("Queried ") + std::to_string(positions.size()) + " positions");
    return positions;
}

double FutuExchange::getAvailableFunds() {
    AccountInfo info = getAccountInfo();
    return info.available_funds;
}

// ========== 交易相关 ==========

std::string FutuExchange::placeOrder(
    const std::string& symbol,
    const std::string& side,
    int quantity,
    const std::string& order_type,
    double price) {
    
    if (!connected_) {
        LOG_ERROR("Not connected to exchange");
        return "";
    }
    
    std::stringstream ss;
    ss << "Placing order: " << symbol << " " << side << " " << quantity;
    if (order_type == "LIMIT") {
        ss << " @ " << price;
    }
    LOG_INFO(ss.str());
    
    std::string order_id = "";
    
    #ifdef ENABLE_FUTU
    if (trd_api_ == nullptr || spi_ == nullptr) {
        LOG_ERROR("Trd API not initialized");
        return order_id;
    }
    
    if (account_ids_.empty()) {
        LOG_ERROR("No account available");
        return order_id;
    }
    
    try {
        // 使用第一个账户
        uint64_t acc_id = account_ids_[0];
        
        Trd_PlaceOrder::Request req;
        auto* c2s = req.mutable_c2s();
        
        // 设置交易头
        auto* header = c2s->mutable_header();
        header->set_trdenv(config_.is_simulation ? Trd_Common::TrdEnv_Simulate : Trd_Common::TrdEnv_Real);
        header->set_accid(acc_id);
        header->set_trdmarket(Trd_Common::TrdMarket_HK);
        
        // 设置交易方向
        if (side == "BUY") {
            c2s->set_trdside(Trd_Common::TrdSide_Buy);
        } else {
            c2s->set_trdside(Trd_Common::TrdSide_Sell);
        }
        
        // 设置订单类型
        if (order_type == "MARKET") {
            c2s->set_ordertype(Trd_Common::OrderType_Market);
        } else {
            c2s->set_ordertype(Trd_Common::OrderType_Normal);
            c2s->set_price(price);
        }
        
        // 解析symbol获取股票代码
        std::string code = symbol;
        size_t dot_pos = symbol.find('.');
        if (dot_pos != std::string::npos) {
            code = symbol.substr(dot_pos + 1);
        }
        
        c2s->set_code(code);
        c2s->set_qty(quantity);
        c2s->set_secmarket(Trd_Common::TrdSecMarket_HK);
        c2s->set_adjustprice(true);  // 自动调整价格到合法范围
        
        u32_t serial_no = trd_api_->PlaceOrder(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send place order request");
            return order_id;
        }
        
        // 等待响应
        if (!spi_->WaitForReply(serial_no, 5000)) {
            LOG_ERROR("Place order timeout");
            return order_id;
        }
        
        // 从响应中提取订单ID
        {
            std::lock_guard<std::mutex> lock(spi_->mutex_);
            auto it = spi_->place_order_responses_.find(serial_no);
            if (it != spi_->place_order_responses_.end()) {
                const auto& rsp = it->second;
                if (rsp.rettype() >= 0 && rsp.has_s2c()) {
                    order_id = std::to_string(rsp.s2c().orderid());
                    LOG_INFO(std::string("Order placed successfully: ") + order_id);
                } else {
                    LOG_ERROR(std::string("Place order failed: ") + rsp.retmsg());
                }
                spi_->place_order_responses_.erase(it);
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during place order: ") + e.what());
    }
    #endif
    
    return order_id;
}

bool FutuExchange::cancelOrder(const std::string& order_id) {
    if (!connected_) {
        LOG_ERROR("Not connected to exchange");
        return false;
    }
    
    #ifdef ENABLE_FUTU
    if (trd_api_ == nullptr || spi_ == nullptr) {
        LOG_ERROR("Trd API not initialized");
        return false;
    }
    
    if (account_ids_.empty()) {
        LOG_ERROR("No account available");
        return false;
    }
    
    try {
        uint64_t acc_id = account_ids_[0];
        uint64_t order_id_num = std::stoull(order_id);
        
        Trd_ModifyOrder::Request req;
        auto* c2s = req.mutable_c2s();
        
        // 设置交易头
        auto* header = c2s->mutable_header();
        header->set_trdenv(config_.is_simulation ? Trd_Common::TrdEnv_Simulate : Trd_Common::TrdEnv_Real);
        header->set_accid(acc_id);
        header->set_trdmarket(Trd_Common::TrdMarket_HK);
        
        // 设置订单ID
        c2s->set_orderid(order_id_num);
        
        // 设置修改操作为撤单
        c2s->set_modifyorderop(Trd_Common::ModifyOrderOp_Cancel);
        
        u32_t serial_no = trd_api_->ModifyOrder(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send cancel order request");
            return false;
        }
        
        // 等待响应
        if (!spi_->WaitForReply(serial_no, 5000)) {
            LOG_ERROR("Cancel order timeout");
            return false;
        }
        
        LOG_INFO(std::string("Order cancelled: ") + order_id);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during cancel order: ") + e.what());
        return false;
    }
    #else
    LOG_INFO(std::string("Order cancelled (simulation): ") + order_id);
    return true;
    #endif
}

bool FutuExchange::modifyOrder(const std::string& order_id, int new_quantity, double new_price) {
    if (!connected_) {
        LOG_ERROR("Not connected to exchange");
        return false;
    }
    
    #ifdef ENABLE_FUTU
    if (trd_api_ == nullptr || spi_ == nullptr) {
        LOG_ERROR("Trd API not initialized");
        return false;
    }
    
    if (account_ids_.empty()) {
        LOG_ERROR("No account available");
        return false;
    }
    
    try {
        uint64_t acc_id = account_ids_[0];
        uint64_t order_id_num = std::stoull(order_id);
        
        Trd_ModifyOrder::Request req;
        auto* c2s = req.mutable_c2s();
        
        // 设置交易头
        auto* header = c2s->mutable_header();
        header->set_trdenv(config_.is_simulation ? Trd_Common::TrdEnv_Simulate : Trd_Common::TrdEnv_Real);
        header->set_accid(acc_id);
        header->set_trdmarket(Trd_Common::TrdMarket_HK);
        
        // 设置订单ID
        c2s->set_orderid(order_id_num);
        
        // 设置修改操作
        c2s->set_modifyorderop(Trd_Common::ModifyOrderOp_Normal);
        
        // 设置新的价格和数量
        c2s->set_price(new_price);
        c2s->set_qty(new_quantity);
        c2s->set_adjustprice(true);
        
        u32_t serial_no = trd_api_->ModifyOrder(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send modify order request");
            return false;
        }
        
        // 等待响应
        if (!spi_->WaitForReply(serial_no, 5000)) {
            LOG_ERROR("Modify order timeout");
            return false;
        }
        
        std::stringstream ss;
        ss << "Order modified: " << order_id << " new_qty=" << new_quantity << " new_price=" << new_price;
        LOG_INFO(ss.str());
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during modify order: ") + e.what());
        return false;
    }
    #else
    std::stringstream ss;
    ss << "Order modified (simulation): " << order_id << " new_qty=" << new_quantity << " new_price=" << new_price;
    LOG_INFO(ss.str());
    return true;
    #endif
}

OrderData FutuExchange::getOrderStatus(const std::string& order_id) {
    OrderData order;
    order.order_id = order_id;
    order.status = OrderStatus::SUBMITTED;
    
    #ifdef ENABLE_FUTU
    // TODO: 实现查询订单状态
    // 可以使用 GetOrderList 接口查询当日订单
    #endif
    
    return order;
}

std::vector<OrderData> FutuExchange::getOrderHistory(int days) {
    (void)days;
    
    #ifdef ENABLE_FUTU
    // TODO: 实现查询历史订单
    // 使用 GetHistoryOrderList 接口
    #endif
    
    return {};
}

// ========== 行情数据相关 ==========

bool FutuExchange::subscribeKLine(const std::string& symbol, const std::string& kline_type) {
    if (!connected_) {
        LOG_ERROR("Not connected to exchange");
        return false;
    }
    
    #ifdef ENABLE_FUTU
    if (qot_api_ == nullptr || spi_ == nullptr) {
        LOG_ERROR("Qot API not initialized");
        return false;
    }
    
    try {
        Qot_Sub::Request req;
        auto* c2s = req.mutable_c2s();
        
        // 添加要订阅的股票
        auto* security = c2s->add_securitylist();
        *security = convertToSecurity(symbol);
        
        // 添加订阅类型 - K线
        int32_t kl_type = convertKLineType(kline_type);
        c2s->add_subtypelist(kl_type);
        
        // 设置为订阅
        c2s->set_issuborunsub(true);
        c2s->set_isregorunregpush(true);
        
        u32_t serial_no = qot_api_->Sub(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send subscribe KLine request");
            return false;
        }
        
        // 等待响应
        if (!spi_->WaitForReply(serial_no, 5000)) {
            LOG_ERROR("Subscribe KLine timeout");
            return false;
        }
        
        std::stringstream ss;
        ss << "Subscribed KLine: " << symbol << " " << kline_type;
        LOG_INFO(ss.str());
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during subscribe KLine: ") + e.what());
        return false;
    }
    #else
    std::stringstream ss;
    ss << "Subscribed KLine (simulation): " << symbol << " " << kline_type;
    LOG_INFO(ss.str());
    return true;
    #endif
}

bool FutuExchange::unsubscribeKLine(const std::string& symbol) {
    if (!connected_) {
        return false;
    }
    
    #ifdef ENABLE_FUTU
    if (qot_api_ == nullptr) {
        return false;
    }
    
    try {
        Qot_Sub::Request req;
        auto* c2s = req.mutable_c2s();
        
        // 添加要取消订阅的股票
        auto* security = c2s->add_securitylist();
        *security = convertToSecurity(symbol);
        
        // 设置为取消订阅
        c2s->set_issuborunsub(false);
        
        u32_t serial_no = qot_api_->Sub(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send unsubscribe KLine request");
            return false;
        }
        
        std::stringstream ss;
        ss << "Unsubscribed KLine: " << symbol;
        LOG_INFO(ss.str());
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during unsubscribe KLine: ") + e.what());
        return false;
    }
    #else
    std::stringstream ss;
    ss << "Unsubscribed KLine (simulation): " << symbol;
    LOG_INFO(ss.str());
    return true;
    #endif
}

bool FutuExchange::subscribeTick(const std::string& symbol) {
    if (!connected_) {
        LOG_ERROR("Not connected to exchange");
        return false;
    }
    
    #ifdef ENABLE_FUTU
    if (qot_api_ == nullptr || spi_ == nullptr) {
        LOG_ERROR("Qot API not initialized");
        return false;
    }
    
    try {
        Qot_Sub::Request req;
        auto* c2s = req.mutable_c2s();
        
        // 添加要订阅的股票
        auto* security = c2s->add_securitylist();
        *security = convertToSecurity(symbol);
        
        // 添加订阅类型 - 基础报价(相当于Tick)
        c2s->add_subtypelist(Qot_Common::SubType_Basic);
        c2s->add_subtypelist(Qot_Common::SubType_Ticker);
        
        // 设置为订阅
        c2s->set_issuborunsub(true);
        c2s->set_isregorunregpush(true);
        
        u32_t serial_no = qot_api_->Sub(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send subscribe Tick request");
            return false;
        }
        
        // 等待响应
        if (!spi_->WaitForReply(serial_no, 5000)) {
            LOG_ERROR("Subscribe Tick timeout");
            return false;
        }
        
        std::stringstream ss;
        ss << "Subscribed Tick: " << symbol;
        LOG_INFO(ss.str());
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during subscribe Tick: ") + e.what());
        return false;
    }
    #else
    std::stringstream ss;
    ss << "Subscribed Tick (simulation): " << symbol;
    LOG_INFO(ss.str());
    return true;
    #endif
}

bool FutuExchange::unsubscribeTick(const std::string& symbol) {
    (void)symbol;
    
    if (!connected_) {
        return false;
    }
    
    // TODO: 实现取消Tick订阅
    return true;
}

std::vector<KlineData> FutuExchange::getHistoryKLine(
    const std::string& symbol,
    const std::string& kline_type,
    int count) {
    
    if (!connected_) {
        LOG_ERROR("Not connected to exchange");
        return {};
    }
    
    std::vector<KlineData> klines;
    
    #ifdef ENABLE_FUTU
    if (qot_api_ == nullptr || spi_ == nullptr) {
        LOG_ERROR("Qot API not initialized");
        return klines;
    }
    
    try {
        Qot_RequestHistoryKL::Request req;
        auto* c2s = req.mutable_c2s();
        
        // 设置股票
        auto* security = c2s->mutable_security();
        *security = convertToSecurity(symbol);
        
        // 设置K线类型
        c2s->set_kltype(convertKLineType(kline_type));
        
        // 设置请求数量
        c2s->set_maxackklnum(count);
        
        // 设置复权类型
        c2s->set_rehabtype(Qot_Common::RehabType_None);
        
        u32_t serial_no = qot_api_->RequestHistoryKL(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send get history KLine request");
            return klines;
        }
        
        // 等待响应
        if (!spi_->WaitForReply(serial_no, 10000)) {
            LOG_ERROR("Get history KLine timeout");
            return klines;
        }
        
        // 从响应中提取K线数据
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
                        kline.exchange = "Futu";
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
        LOG_ERROR(std::string("Exception during get history KLine: ") + e.what());
    }
    #endif
    
    LOG_INFO(std::string("Got ") + std::to_string(klines.size()) + " history KLines");
    return klines;
}

Snapshot FutuExchange::getSnapshot(const std::string& symbol) {
    if (!connected_) {
        LOG_ERROR("Not connected to exchange");
        return Snapshot();
    }
    
    Snapshot snapshot;
    
    #ifdef ENABLE_FUTU
    if (qot_api_ == nullptr || spi_ == nullptr) {
        LOG_ERROR("Qot API not initialized");
        return snapshot;
    }
    
    try {
        Qot_GetSecuritySnapshot::Request req;
        auto* c2s = req.mutable_c2s();
        
        // 添加要查询的股票
        auto* security = c2s->add_securitylist();
        *security = convertToSecurity(symbol);
        
        u32_t serial_no = qot_api_->GetSecuritySnapshot(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send get snapshot request");
            return snapshot;
        }
        
        // 等待响应
        if (!spi_->WaitForReply(serial_no, 5000)) {
            LOG_ERROR("Get snapshot timeout");
            return snapshot;
        }
        
        // 从响应中提取快照数据
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
                        snapshot.exchange = "Futu";
                        snapshot.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
                        snapshot.last_price = basic.curprice();
                        snapshot.open_price = basic.openprice();
                        snapshot.high_price = basic.highprice();
                        snapshot.low_price = basic.lowprice();
                        snapshot.pre_close = basic.lastcloseprice();
                        snapshot.volume = basic.volume();
                        snapshot.turnover = basic.turnover();
                        snapshot.turnover_rate = basic.turnoverrate();
                        snapshot.price_change = basic.has_pricespread() ? basic.pricespread() : 0.0;
                        snapshot.price_change_abs = 0.0;  // 富途API没有提供绝对变化值
                    }
                }
                spi_->snapshot_responses_.erase(it);
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during get snapshot: ") + e.what());
    }
    #endif
    
    return snapshot;
}

// ========== 市场扫描相关 ==========

std::vector<std::string> FutuExchange::getMarketStockList(const std::string& market) {
    if (!connected_) {
        LOG_ERROR("Not connected to exchange");
        return {};
    }
    
    std::vector<std::string> stocks;
    
    #ifdef ENABLE_FUTU
    if (qot_api_ == nullptr || spi_ == nullptr) {
        LOG_ERROR("Qot API not initialized");
        return stocks;
    }
    
    try {
        // 使用 GetPlateSecurity 接口获取板块下的股票
        // 或者使用 GetStaticInfo 获取市场股票信息
        
        // 这里简化实现，返回一些常见股票
        LOG_WARNING("getMarketStockList not fully implemented, returning sample stocks");
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during get market stock list: ") + e.what());
    }
    #endif
    
    // 返回一些示例股票代码
    stocks.push_back("00700");  // 腾讯
    stocks.push_back("09988");  // 阿里巴巴
    stocks.push_back("03690");  // 美团
    stocks.push_back("01810");  // 小米
    stocks.push_back("02318");  // 平安
    
    return stocks;
}

std::map<std::string, Snapshot> FutuExchange::getBatchSnapshots(
    const std::vector<std::string>& stock_codes) {
    
    if (!connected_) {
        LOG_ERROR("Not connected to exchange");
        return {};
    }
    
    std::map<std::string, Snapshot> snapshots;
    
    #ifdef ENABLE_FUTU
    if (qot_api_ == nullptr || spi_ == nullptr) {
        LOG_ERROR("Qot API not initialized");
        return snapshots;
    }
    
    try {
        Qot_GetSecuritySnapshot::Request req;
        auto* c2s = req.mutable_c2s();
        
        // 添加要查询的所有股票
        for (const auto& code : stock_codes) {
            auto* security = c2s->add_securitylist();
            *security = convertToSecurity(code);
        }
        
        u32_t serial_no = qot_api_->GetSecuritySnapshot(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send batch get snapshot request");
            return snapshots;
        }
        
        // 等待响应
        if (!spi_->WaitForReply(serial_no, 10000)) {
            LOG_ERROR("Batch get snapshot timeout");
            return snapshots;
        }
        
        // 从响应中提取快照数据
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
                        snapshot.exchange = "Futu";
                        snapshot.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
                        snapshot.last_price = basic.curprice();
                        snapshot.open_price = basic.openprice();
                        snapshot.high_price = basic.highprice();
                        snapshot.low_price = basic.lowprice();
                        snapshot.pre_close = basic.lastcloseprice();
                        snapshot.volume = basic.volume();
                        snapshot.turnover = basic.turnover();
                        snapshot.turnover_rate = basic.turnoverrate();
                        snapshot.price_change = basic.has_pricespread() ? basic.pricespread() : 0.0;
                        snapshot.price_change_abs = 0.0;  // 富途API没有提供绝对变化值
                        
                        snapshots[sec.code()] = snapshot;
                    }
                }
                spi_->snapshot_responses_.erase(it);
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during batch get snapshots: ") + e.what());
    }
    #endif
    
    LOG_INFO(std::string("Got ") + std::to_string(snapshots.size()) + " snapshots");
    return snapshots;
}
