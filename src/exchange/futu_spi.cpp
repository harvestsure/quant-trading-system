#ifdef ENABLE_FUTU

#include "futu_spi.h"
#include "event/event_interface.h"
#include "event/event.h"
#include "exchange/futu_exchange.h"
#include <chrono>

FutuSpi::FutuSpi(FutuExchange* exchange) : exchange_(exchange) {
}

FutuSpi::~FutuSpi() {
}

bool FutuSpi::WaitForReply(Futu::u32_t serial_no, int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto timeout = std::chrono::milliseconds(timeout_ms);
    
    bool result = cv_.wait_for(lock, timeout, [this, serial_no]() {
        return reply_flags_[serial_no];
    });
    
    if (result) {
        reply_flags_.erase(serial_no);
    }
    
    return result;
}

void FutuSpi::NotifyReply(Futu::u32_t serial_no) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        reply_flags_[serial_no] = true;
    }
    cv_.notify_all();
}

// ========== API lifecycle management ==========

bool FutuSpi::InitApi(const std::string& host, int port) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (api_initialized_) {
            writeLog(LogLevel::Warn, "API already initialized");
            return true;
        }
        
        host_ = host;
        port_ = port;
    }  // Release lock because WaitForReply will reacquire it
    
    try {
        // Initialize FTAPI
        Futu::FTAPI::Init();
        
        // Create market data (Qot) API
        qot_api_ = Futu::FTAPI::CreateQotApi();
        if (qot_api_ == nullptr) {
            writeLog(LogLevel::Error, "Failed to create Qot API");
            return false;
        }
        
        // Set client info
        qot_api_->SetClientInfo("QUANT_TRADING_SYSTEM", 1);

        // Register callbacks
        qot_api_->RegisterConnSpi(this);
        qot_api_->RegisterQotSpi(this);

        // Initialize connection
        bool ret = qot_api_->InitConnect(host.c_str(), port, false);
        /*if (!ret) {
            writeLog(LogLevel::Error, "Failed to initialize Qot API connection");
            Futu::FTAPI::ReleaseQotApi(qot_api_);
            qot_api_ = nullptr;
            return false;
        }*/

        WaitForReply(0, 5000); // Wait for connection initialization completion, serial_no 0 indicates init

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!is_qot_connected_) {
                writeLog(LogLevel::Error, "Qot API connection failed");
                Futu::FTAPI::ReleaseQotApi(qot_api_);
                qot_api_ = nullptr;
                return false;
            }
        }
        
        // Create trade (Trd) API
        trd_api_ = Futu::FTAPI::CreateTrdApi();
        if (trd_api_ == nullptr) {
            writeLog(LogLevel::Error, "Failed to create Trd API");
            qot_api_->Close();
            Futu::FTAPI::ReleaseQotApi(qot_api_);
            qot_api_ = nullptr;
            return false;
        }
        
        // Set client info
        trd_api_->SetClientInfo("QUANT_TRADING_SYSTEM", 1);

        // Register callbacks
        trd_api_->RegisterConnSpi(this);
        trd_api_->RegisterTrdSpi(this);

        // Initialize connection
        ret = trd_api_->InitConnect(host.c_str(), port, false);
        /* if (!ret) {
            writeLog(LogLevel::Error, "Failed to initialize Trd API connection");
            qot_api_->Close();
            Futu::FTAPI::ReleaseQotApi(qot_api_);
            Futu::FTAPI::ReleaseTrdApi(trd_api_);
            qot_api_ = nullptr;
            trd_api_ = nullptr;
            return false;
        }*/

        WaitForReply(0, 5000); // Wait for connection initialization completion, serial_no 0 indicates init

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!is_trd_connected_) {
                writeLog(LogLevel::Error, "Trd API connection failed");
                trd_api_->Close();
                qot_api_->Close();
                Futu::FTAPI::ReleaseQotApi(qot_api_);
                Futu::FTAPI::ReleaseTrdApi(trd_api_);
                qot_api_ = nullptr;
                trd_api_ = nullptr;
                return false;
            }
            
            api_initialized_ = true;
        }
        
        writeLog(LogLevel::Info, std::string("FTAPI initialized successfully at ") + host + ":" + std::to_string(port));
        return true;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during API initialization: ") + e.what());
        return false;
    }
}

void FutuSpi::ReleaseApi() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!api_initialized_) {
        return;
    }
    
    try {
        if (qot_api_ != nullptr) {
            qot_api_->Close();
            Futu::FTAPI::ReleaseQotApi(qot_api_);
            qot_api_ = nullptr;
        }
        
        if (trd_api_ != nullptr) {
            trd_api_->Close();
            Futu::FTAPI::ReleaseTrdApi(trd_api_);
            trd_api_ = nullptr;
        }
        
        api_initialized_ = false;
        writeLog(LogLevel::Info, "FTAPI released successfully");
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during API release: ") + e.what());
    }
}

bool FutuSpi::IsConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return api_initialized_ && qot_api_ != nullptr && trd_api_ != nullptr;
}

// ========== Trade request helper methods ==========

Futu::u32_t FutuSpi::SendUnlockTrade(const std::string& password) {
    if (trd_api_ == nullptr) {
        writeLog(LogLevel::Error, "Trd API not initialized");
        return 0;
    }
    
    try {
        Trd_UnlockTrade::Request req;
        auto* c2s = req.mutable_c2s();
        c2s->set_unlock(true);
        c2s->set_pwdmd5(password);
        c2s->set_securityfirm(Trd_Common::SecurityFirm_FutuSecurities);
        
        Futu::u32_t serial_no = trd_api_->UnlockTrade(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send unlock trade request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent unlock trade request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send unlock trade: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetAccList() {
    if (trd_api_ == nullptr) {
        writeLog(LogLevel::Error, "Trd API not initialized");
        return 0;
    }
    
    try {
        Trd_GetAccList::Request req;
        auto* c2s = req.mutable_c2s();
        c2s->set_userid(0);  // 0 indicates the current connected user
        
        Futu::u32_t serial_no = trd_api_->GetAccList(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send get account list request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent get account list request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send get account list: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetFunds(Futu::u64_t acc_id, int trd_env, int trd_market) {
    if (trd_api_ == nullptr) {
        writeLog(LogLevel::Error, "Trd API not initialized");
        return 0;
    }
    
    try {
        Trd_GetFunds::Request req;
        auto* c2s = req.mutable_c2s();
        auto* header = c2s->mutable_header();
        header->set_trdenv(trd_env);
        header->set_accid(acc_id);
        header->set_trdmarket(trd_market);
        
        Futu::u32_t serial_no = trd_api_->GetFunds(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send get funds request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent get funds request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send get funds: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetPositionList(Futu::u64_t acc_id, int trd_env, int trd_market) {
    if (trd_api_ == nullptr) {
        writeLog(LogLevel::Error, "Trd API not initialized");
        return 0;
    }
    
    try {
        Trd_GetPositionList::Request req;
        auto* c2s = req.mutable_c2s();
        auto* header = c2s->mutable_header();
        header->set_trdenv(trd_env);
        header->set_accid(acc_id);
        header->set_trdmarket(trd_market);
        
        Futu::u32_t serial_no = trd_api_->GetPositionList(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send get position list request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent get position list request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send get position list: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetOrderList(Futu::u64_t acc_id, int trd_env, int trd_market) {
    if (trd_api_ == nullptr) {
        writeLog(LogLevel::Error, "Trd API not initialized");
        return 0;
    }
    
    try {
        Trd_GetOrderList::Request req;
        auto* c2s = req.mutable_c2s();
        auto* header = c2s->mutable_header();
        header->set_trdenv(trd_env);
        header->set_accid(acc_id);
        header->set_trdmarket(trd_market);
        
        Futu::u32_t serial_no = trd_api_->GetOrderList(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send get order list request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent get order list request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send get order list: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendPlaceOrder(Futu::u64_t acc_id, int trd_env, int trd_market,
                                     const Qot_Common::Security& security, int order_side,
                                     int order_type, Futu::i64_t quantity, double price) {
    if (trd_api_ == nullptr) {
        writeLog(LogLevel::Error, "Trd API not initialized");
        return 0;
    }
    
    try {
        Trd_PlaceOrder::Request req;
        auto* c2s = req.mutable_c2s();
        auto* header = c2s->mutable_header();
        header->set_trdenv(trd_env);
        header->set_accid(acc_id);
        header->set_trdmarket(trd_market);
        
        // Set order parameters (set directly in C2S)
        c2s->set_trdside(order_side);
        c2s->set_ordertype(order_type);
        c2s->set_code(security.code());
        c2s->set_qty((double)quantity);
        if (price > 0) {
            c2s->set_price(price);
        }
        
        Futu::u32_t serial_no = trd_api_->PlaceOrder(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send place order request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent place order request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send place order: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendModifyOrder(Futu::u64_t acc_id, int trd_env, Futu::u64_t order_id,
                                      Futu::i64_t quantity, double price) {
    if (trd_api_ == nullptr) {
        writeLog(LogLevel::Error, "Trd API not initialized");
        return 0;
    }
    
    try {
        Trd_ModifyOrder::Request req;
        auto* c2s = req.mutable_c2s();
        auto* header = c2s->mutable_header();
        header->set_trdenv(trd_env);
        header->set_accid(acc_id);
        header->set_trdmarket(Trd_Common::TrdMarket_HK);  // Default to HK market
        
        c2s->set_orderid(order_id);
        c2s->set_qty(quantity);
        c2s->set_price(price);
        
        Futu::u32_t serial_no = trd_api_->ModifyOrder(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send modify order request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent modify order request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send modify order: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendCancelOrder(Futu::u64_t acc_id, int trd_env, Futu::u64_t order_id) {
    if (trd_api_ == nullptr) {
        writeLog(LogLevel::Error, "Trd API not initialized");
        return 0;
    }
    
    try {
        // Use ModifyOrder to cancel order (by setting Order Status to Cancelled)
        Trd_ModifyOrder::Request req;
        auto* c2s = req.mutable_c2s();
        auto* header = c2s->mutable_header();
        header->set_trdenv(trd_env);
        header->set_accid(acc_id);
        header->set_trdmarket(Trd_Common::TrdMarket_HK);
        
        c2s->set_orderid(order_id);
        
        Futu::u32_t serial_no = trd_api_->ModifyOrder(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send cancel order request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent cancel order request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send cancel order: ") + e.what());
        return 0;
    }
}

// ========== Market data request helper methods ==========

Futu::u32_t FutuSpi::SendSubscribeKLine(const Qot_Common::Security& security, int kline_type) {
    if (qot_api_ == nullptr) {
        writeLog(LogLevel::Error, "Qot API not initialized");
        return 0;
    }
    
    try {
        Qot_Sub::Request req;
        auto* c2s = req.mutable_c2s();
        
        // Add the security to subscribe
        auto* security_item = c2s->add_securitylist();
        *security_item = security;
        
        // Add subscription type - KLine
        // Use integer values like 1,2,3... or refer to Qot_Common for specific values
        c2s->add_subtypelist(1 + kline_type);  // SubType_K_1Min = 1, 1Min=0, 3Min=1 etc.
        
        // Set as subscription
        c2s->set_issuborunsub(true);
        c2s->set_isregorunregpush(true);
        
        Futu::u32_t serial_no = qot_api_->Sub(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send subscribe KLine request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent subscribe KLine request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send subscribe KLine: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetKLine(const Qot_Common::Security& security, int kline_type, int count) {
    if (qot_api_ == nullptr) {
        writeLog(LogLevel::Error, "Qot API not initialized");
        return 0;
    }
    
    try {
        Qot_GetKL::Request req;
        auto* c2s = req.mutable_c2s();
        *c2s->mutable_security() = security;
        c2s->set_kltype(kline_type);
        c2s->set_reqnum(count);
        
        Futu::u32_t serial_no = qot_api_->GetKL(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send get KLine request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent get KLine request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send get KLine: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetHistoryKLine(const Qot_Common::Security& security, int kline_type, int count) {
    if (qot_api_ == nullptr) {
        writeLog(LogLevel::Error, "Qot API not initialized");
        return 0;
    }
    
    try {
        Qot_RequestHistoryKL::Request req;
        auto* c2s = req.mutable_c2s();
        *c2s->mutable_security() = security;
        c2s->set_kltype(kline_type);
        c2s->set_maxackklnum(count);
        c2s->set_rehabtype(1);  // Forward-adjusted
        
        // Calculate appropriate time range based on K-line type
        auto now = std::chrono::system_clock::now();
        auto end_time = now;
        
        // Shift backward a sufficient time window
        int days_back = 30;  // Default 30 days
        if (kline_type <= 8) {  // Minute-level K-lines (1min~60min)
            days_back = 5;
        } else if (kline_type == 9 || kline_type == 10) {  // Daily/Weekly K
            days_back = count * 2 + 10;  // Add buffer for non-trading days
        } else {
            days_back = count * 40;  // Monthly K, etc.
        }
        
        auto begin_time = now - std::chrono::hours(24 * days_back);
        
        // Format time string yyyy-MM-dd HH:mm:ss
        auto format_time = [](std::chrono::system_clock::time_point tp) -> std::string {
            auto time_t_val = std::chrono::system_clock::to_time_t(tp);
            struct tm tm_val;
            localtime_r(&time_t_val, &tm_val);
            char buf[32];
            snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                     tm_val.tm_year + 1900, tm_val.tm_mon + 1, tm_val.tm_mday,
                     tm_val.tm_hour, tm_val.tm_min, tm_val.tm_sec);
            return std::string(buf);
        };
        
        c2s->set_begintime(format_time(begin_time));
        c2s->set_endtime(format_time(end_time));
        
        Futu::u32_t serial_no = qot_api_->RequestHistoryKL(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send request history KLine request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent request history KLine, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send request history KLine: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetSecuritySnapshot(const std::vector<Qot_Common::Security>& securities) {
    if (qot_api_ == nullptr) {
        writeLog(LogLevel::Error, "Qot API not initialized");
        return 0;
    }
    
    try {
        Qot_GetSecuritySnapshot::Request req;
        auto* c2s = req.mutable_c2s();
        
        for (const auto& security : securities) {
            auto* sec = c2s->add_securitylist();
            *sec = security;
        }
        
        Futu::u32_t serial_no = qot_api_->GetSecuritySnapshot(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send get security snapshot request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent get security snapshot request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send get security snapshot: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetPlateSecurity(const std::string& plate_code) {
    if (qot_api_ == nullptr) {
        writeLog(LogLevel::Error, "Qot API not initialized");
        return 0;
    }
    
    try {
        Qot_GetPlateSecurity::Request req;
        auto* c2s = req.mutable_c2s();
        auto* plate = c2s->mutable_plate();
        plate->set_market(Qot_Common::QotMarket_HK_Security);
        plate->set_code(plate_code);
        
        Futu::u32_t serial_no = qot_api_->GetPlateSecurity(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send get plate security request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent get plate security request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send get plate security: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetStaticInfo(int market_type, int security_type) {
    if (qot_api_ == nullptr) {
        writeLog(LogLevel::Error, "Qot API not initialized");
        return 0;
    }
    
    try {
        Qot_GetStaticInfo::Request req;
        auto* c2s = req.mutable_c2s();
        
        // Set market type
        c2s->set_market(market_type);

        // Set security type (equity)
        c2s->set_sectype(security_type);
        
        Futu::u32_t serial_no = qot_api_->GetStaticInfo(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send get static info request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent get static info request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send get static info: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendSubscribeTick(const Qot_Common::Security& security) {
    if (qot_api_ == nullptr) {
        writeLog(LogLevel::Error, "Qot API not initialized");
        return 0;
    }
    
    try {
        Qot_Sub::Request req;
        auto* c2s = req.mutable_c2s();
        
        // Add the security to subscribe
        auto* security_item = c2s->add_securitylist();
        *security_item = security;

        // Add subscription types - Basic quote and Ticker
        c2s->add_subtypelist(Qot_Common::SubType_Basic);
        c2s->add_subtypelist(Qot_Common::SubType_Ticker);

        // Set as subscription
        c2s->set_issuborunsub(true);
        c2s->set_isregorunregpush(true);
        
        Futu::u32_t serial_no = qot_api_->Sub(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send subscribe Tick request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent subscribe Tick request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send subscribe Tick: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendUnsubscribeKLine(const Qot_Common::Security& security) {
    if (qot_api_ == nullptr) {
        writeLog(LogLevel::Error, "Qot API not initialized");
        return 0;
    }
    
    try {
        Qot_Sub::Request req;
        auto* c2s = req.mutable_c2s();
        
        // Add the security to unsubscribe
        auto* security_item = c2s->add_securitylist();
        *security_item = security;
        
        // Set to unsubscribe
        c2s->set_issuborunsub(false);
        
        Futu::u32_t serial_no = qot_api_->Sub(req);
        if (serial_no == 0) {
            writeLog(LogLevel::Error, "Failed to send unsubscribe KLine request");
            return 0;
        }
        
        writeLog(LogLevel::Info, std::string("Sent unsubscribe KLine request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception during send unsubscribe KLine: ") + e.what());
        return 0;
    }
}

// ========== FTSPI_Conn callbacks ==========

void FutuSpi::OnInitConnect(Futu::FTAPI_Conn* pConn, Futu::i64_t nErrCode, const char* strDesc) {
    if (nErrCode == 0) {
        writeLog(LogLevel::Info, std::string("FTAPI connected successfully: ") + strDesc);
    } else {
        writeLog(LogLevel::Error, std::string("FTAPI connection failed: code=") + std::to_string(nErrCode) + ", desc=" + strDesc);
    }

    if (pConn == qot_api_) {
		is_qot_connected_ = (nErrCode == 0);
    } else if (pConn == trd_api_) {
		is_trd_connected_ = (nErrCode == 0);
	}

    NotifyReply(0); // Use serial_no 0 to indicate connection initialization complete
}

void FutuSpi::OnDisConnect(Futu::FTAPI_Conn* pConn, Futu::i64_t nErrCode) {
    writeLog(LogLevel::Warn, std::string("FTAPI disconnected: code=") + std::to_string(nErrCode));
}

// ========== FTSPI_Qot callbacks ==========

void FutuSpi::OnReply_GetGlobalState(Futu::u32_t nSerialNo, const GetGlobalState::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetGlobalState");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_Sub(Futu::u32_t nSerialNo, const Qot_Sub::Response &stRsp) {
    if (stRsp.rettype() >= 0) {
        writeLog(LogLevel::Info, "Subscribe successful");
    } else {
        writeLog(LogLevel::Error, std::string("Subscribe failed: ") + stRsp.retmsg());
    }
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_RegQotPush(Futu::u32_t nSerialNo, const Qot_RegQotPush::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_RegQotPush");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetSubInfo(Futu::u32_t nSerialNo, const Qot_GetSubInfo::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetSubInfo");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetTicker(Futu::u32_t nSerialNo, const Qot_GetTicker::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetTicker");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetBasicQot(Futu::u32_t nSerialNo, const Qot_GetBasicQot::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetBasicQot");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetOrderBook(Futu::u32_t nSerialNo, const Qot_GetOrderBook::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetOrderBook");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetKL(Futu::u32_t nSerialNo, const Qot_GetKL::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        kline_responses_[nSerialNo] = stRsp;
    }
    writeLog(LogLevel::Info, "OnReply_GetKL");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetRT(Futu::u32_t nSerialNo, const Qot_GetRT::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetRT");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetBroker(Futu::u32_t nSerialNo, const Qot_GetBroker::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetBroker");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_RequestRehab(Futu::u32_t nSerialNo, const Qot_RequestRehab::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_RequestRehab");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_RequestHistoryKL(Futu::u32_t nSerialNo, const Qot_RequestHistoryKL::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        history_kline_responses_[nSerialNo] = stRsp;
    }
    writeLog(LogLevel::Info, "OnReply_RequestHistoryKL");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_RequestHistoryKLQuota(Futu::u32_t nSerialNo, const Qot_RequestHistoryKLQuota::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_RequestHistoryKLQuota");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetTradeDate(Futu::u32_t nSerialNo, const Qot_GetTradeDate::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetTradeDate");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetStaticInfo(Futu::u32_t nSerialNo, const Qot_GetStaticInfo::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        static_info_responses_[nSerialNo] = stRsp;
    }
    writeLog(LogLevel::Info, "OnReply_GetStaticInfo");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetSecuritySnapshot(Futu::u32_t nSerialNo, const Qot_GetSecuritySnapshot::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_responses_[nSerialNo] = stRsp;
    }
    writeLog(LogLevel::Info, "OnReply_GetSecuritySnapshot");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetPlateSet(Futu::u32_t nSerialNo, const Qot_GetPlateSet::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetPlateSet");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetPlateSecurity(Futu::u32_t nSerialNo, const Qot_GetPlateSecurity::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        plate_security_responses_[nSerialNo] = stRsp;
    }
    writeLog(LogLevel::Info, "OnReply_GetPlateSecurity");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetReference(Futu::u32_t nSerialNo, const Qot_GetReference::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetReference");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetOwnerPlate(Futu::u32_t nSerialNo, const Qot_GetOwnerPlate::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetOwnerPlate");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetHoldingChangeList(Futu::u32_t nSerialNo, const Qot_GetHoldingChangeList::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetHoldingChangeList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetOptionChain(Futu::u32_t nSerialNo, const Qot_GetOptionChain::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetOptionChain");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetWarrant(Futu::u32_t nSerialNo, const Qot_GetWarrant::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetWarrant");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetCapitalFlow(Futu::u32_t nSerialNo, const Qot_GetCapitalFlow::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetCapitalFlow");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetCapitalDistribution(Futu::u32_t nSerialNo, const Qot_GetCapitalDistribution::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetCapitalDistribution");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetUserSecurity(Futu::u32_t nSerialNo, const Qot_GetUserSecurity::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetUserSecurity");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_ModifyUserSecurity(Futu::u32_t nSerialNo, const Qot_ModifyUserSecurity::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_ModifyUserSecurity");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_StockFilter(Futu::u32_t nSerialNo, const Qot_StockFilter::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_StockFilter");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetCodeChange(Futu::u32_t nSerialNo, const Qot_GetCodeChange::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetCodeChange");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetIpoList(Futu::u32_t nSerialNo, const Qot_GetIpoList::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetIpoList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetFutureInfo(Futu::u32_t nSerialNo, const Qot_GetFutureInfo::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetFutureInfo");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_RequestTradeDate(Futu::u32_t nSerialNo, const Qot_RequestTradeDate::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_RequestTradeDate");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_SetPriceReminder(Futu::u32_t nSerialNo, const Qot_SetPriceReminder::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_SetPriceReminder");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetPriceReminder(Futu::u32_t nSerialNo, const Qot_GetPriceReminder::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetPriceReminder");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetUserSecurityGroup(Futu::u32_t nSerialNo, const Qot_GetUserSecurityGroup::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetUserSecurityGroup");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetMarketState(Futu::u32_t nSerialNo, const Qot_GetMarketState::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetMarketState");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetOptionExpirationDate(Futu::u32_t nSerialNo, const Qot_GetOptionExpirationDate::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetOptionExpirationDate");
    NotifyReply(nSerialNo);
}

// Market data pushes
void FutuSpi::OnPush_Notify(const Notify::Response &stRsp) {
    writeLog(LogLevel::Info, "OnPush_Notify");
}

void FutuSpi::OnPush_UpdateBasicQot(const Qot_UpdateBasicQot::Response &stRsp) {
    writeLog(LogLevel::Info, "OnPush_UpdateBasicQot");
    
    try {
        if (stRsp.rettype() != 0) {
            writeLog(LogLevel::Warn, std::string("OnPush_UpdateBasicQot failed: ") + stRsp.retmsg());
            return;
        }
        
        if (!stRsp.has_s2c()) {
            writeLog(LogLevel::Warn, "OnPush_UpdateBasicQot: no s2c data");
            return;
        }
        
        const auto& s2c = stRsp.s2c();
        if (s2c.basicqotlist_size() <= 0) {
            writeLog(LogLevel::Debug, "OnPush_UpdateBasicQot: empty basicqot list");
            return;
        }
        
        // Check exchange_ and event_engine_ validity
        if (exchange_ == nullptr) {
            writeLog(LogLevel::Error, "OnPush_UpdateBasicQot: exchange is null");
            return;
        }
        
        IEventEngine* event_engine = exchange_->getEventEngine();
        if (event_engine == nullptr) {
            writeLog(LogLevel::Warn, "OnPush_UpdateBasicQot: event engine not set");
            return;
        }
        
        // Process each basic market data
        for (int i = 0; i < s2c.basicqotlist_size(); ++i) {
            const auto& basic = s2c.basicqotlist(i);
            if (!basic.has_security()) {
                continue;
            }
            
            const auto& security = basic.security();
            std::string symbol = security.code();
            
            // Construct TickData object
            TickData tick_data;
            tick_data.symbol = symbol;
            tick_data.exchange = exchange_->getName();
            tick_data.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
            tick_data.datetime = basic.updatetime();
            
            // Extract price data from basic
            tick_data.last_price = basic.curprice();
            tick_data.open_price = basic.openprice();
            tick_data.high_price = basic.highprice();
            tick_data.low_price = basic.lowprice();
            tick_data.pre_close = basic.lastcloseprice();
            
            // Volume and turnover
            tick_data.volume = basic.volume();
            tick_data.turnover = basic.turnover();
            tick_data.turnover_rate = basic.turnoverrate();
            
            // Publish Tick event
            auto event = std::make_shared<Event>(EventType::EVENT_TICK);
            event->setData(tick_data);
            event_engine->putEvent(event);
            
            writeLog(LogLevel::Info, std::string("Published TICK event (BasicQot): ") + symbol + " price=" + std::to_string(tick_data.last_price));
        }
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception in OnPush_UpdateBasicQot: ") + e.what());
    }
}

void FutuSpi::OnPush_UpdateOrderBook(const Qot_UpdateOrderBook::Response &stRsp) {
    writeLog(LogLevel::Info, "OnPush_UpdateOrderBook");
}

void FutuSpi::OnPush_UpdateTicker(const Qot_UpdateTicker::Response &stRsp) {
    writeLog(LogLevel::Info, "OnPush_UpdateTicker");
    
    try {
        if (stRsp.rettype() != 0) {
            writeLog(LogLevel::Warn, std::string("OnPush_UpdateTicker failed: ") + stRsp.retmsg());
            return;
        }
        
        if (!stRsp.has_s2c()) {
            writeLog(LogLevel::Warn, "OnPush_UpdateTicker: no s2c data");
            return;
        }
        
        const auto& s2c = stRsp.s2c();
        if (s2c.tickerlist_size() <= 0) {
            writeLog(LogLevel::Debug, "OnPush_UpdateTicker: empty ticker list");
            return;
        }
        
        // Check exchange_ and event_engine_ validity
        if (exchange_ == nullptr) {
            writeLog(LogLevel::Error, "OnPush_UpdateTicker: exchange is null");
            return;
        }
        
        IEventEngine* event_engine = exchange_->getEventEngine();
        if (event_engine == nullptr) {
            writeLog(LogLevel::Warn, "OnPush_UpdateTicker: event engine not set");
            return;
        }
        
        // Get security info (in S2C, not in each ticker)
        if (!s2c.has_security()) {
            writeLog(LogLevel::Warn, "OnPush_UpdateTicker: no security in s2c");
            return;
        }
        
        const auto& security = s2c.security();
        std::string symbol = security.code();
        
        // Process each ticker data
        for (int i = 0; i < s2c.tickerlist_size(); ++i) {
            const auto& ticker = s2c.tickerlist(i);
            
            // Construct TickData object
            TickData tick_data;
            tick_data.symbol = symbol;
            tick_data.exchange = exchange_->getName();
            tick_data.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
            tick_data.datetime = ticker.time();
            
            // Extract trade data from ticker
            tick_data.last_price = ticker.price();           // trade price
            tick_data.volume = ticker.volume();              // volume
            tick_data.turnover = ticker.turnover();          // turnover
            
            // Publish Tick event
            auto event = std::make_shared<Event>(EventType::EVENT_TICK);
            event->setData(tick_data);
            event_engine->putEvent(event);
            
            writeLog(LogLevel::Info, std::string("Published TICK event: ") + symbol + " price=" + std::to_string(tick_data.last_price));
        }
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception in OnPush_UpdateTicker: ") + e.what());
    }
}

void FutuSpi::OnPush_UpdateKL(const Qot_UpdateKL::Response &stRsp) {
    writeLog(LogLevel::Info, "OnPush_UpdateKL");
    
    try {
        if (stRsp.rettype() != 0) {
            writeLog(LogLevel::Warn, std::string("OnPush_UpdateKL failed: ") + stRsp.retmsg());
            return;
        }
        
        if (!stRsp.has_s2c()) {
            writeLog(LogLevel::Warn, "OnPush_UpdateKL: no s2c data");
            return;
        }
        
        const auto& s2c = stRsp.s2c();
        if (s2c.kllist_size() <= 0) {
            writeLog(LogLevel::Debug, "OnPush_UpdateKL: empty kline list");
            return;
        }
        
        // Check exchange_ and event_engine_ validity
        if (exchange_ == nullptr) {
            writeLog(LogLevel::Error, "OnPush_UpdateKL: exchange is null");
            return;
        }
        
        IEventEngine* event_engine = exchange_->getEventEngine();
        if (event_engine == nullptr) {
            writeLog(LogLevel::Warn, "OnPush_UpdateKL: event engine not set");
            return;
        }
        
        // Get K-line type (from first K-line)
        std::string kline_interval = "1m";  // default
        if (s2c.kllist_size() > 0) {
            int32_t kl_type = s2c.kltype();
            // Convert K-line type
            if (kl_type == Qot_Common::KLType_1Min) {
                kline_interval = "1m";
            } else if (kl_type == Qot_Common::KLType_3Min) {
                kline_interval = "3m";
            } else if (kl_type == Qot_Common::KLType_5Min) {
                kline_interval = "5m";
            } else if (kl_type == Qot_Common::KLType_15Min) {
                kline_interval = "15m";
            } else if (kl_type == Qot_Common::KLType_30Min) {
                kline_interval = "30m";
            } else if (kl_type == Qot_Common::KLType_60Min) {
                kline_interval = "1h";
            } else if (kl_type == Qot_Common::KLType_Day) {
                kline_interval = "1d";
            } else if (kl_type == Qot_Common::KLType_Week) {
                kline_interval = "1w";
            } else if (kl_type == Qot_Common::KLType_Month) {
                kline_interval = "1mon";
            }
        }
        
        // Process each K-line data
        for (int i = 0; i < s2c.kllist_size(); ++i) {
            const auto& kl = s2c.kllist(i);
            
            // Get symbol (from security or other fields)
            std::string symbol;
            if (s2c.has_security()) {
                const auto& security = s2c.security();
                symbol = security.code();
            } else {
                // If s2c has no security, continue
                continue;
            }
            
            // Construct KlineData object
            KlineData kline_data;
            kline_data.symbol = symbol;
            kline_data.exchange = exchange_->getName();
            kline_data.interval = kline_interval;
            kline_data.datetime = kl.time();
            
            // Extract price data from K-line
            kline_data.open_price = kl.openprice();
            kline_data.high_price = kl.highprice();
            kline_data.low_price = kl.lowprice();
            kline_data.close_price = kl.closeprice();
            kline_data.volume = kl.volume();
            kline_data.turnover = kl.turnover();
            
            // Parse timestamp (assuming datetime is a string like "2024-02-05 10:00:00")
            // Temporarily set to system current time; should parse from datetime string in production
            kline_data.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
            
            // Set interval_enum based on interval
            if (kline_interval == "1m") {
                kline_data.interval_enum = KlineInterval::K_1M;
            } else if (kline_interval == "3m") {
                kline_data.interval_enum = KlineInterval::K_3M;
            } else if (kline_interval == "5m") {
                kline_data.interval_enum = KlineInterval::K_5M;
            } else if (kline_interval == "15m") {
                kline_data.interval_enum = KlineInterval::K_15M;
            } else if (kline_interval == "30m") {
                kline_data.interval_enum = KlineInterval::K_30M;
            } else if (kline_interval == "1h") {
                kline_data.interval_enum = KlineInterval::K_1H;
            } else if (kline_interval == "1d") {
                kline_data.interval_enum = KlineInterval::K_1D;
            } else if (kline_interval == "1w") {
                kline_data.interval_enum = KlineInterval::K_1W;
            } else if (kline_interval == "1mon") {
                kline_data.interval_enum = KlineInterval::K_1MO;
            }
            
            // Publish KLine event
            auto event = std::make_shared<Event>(EventType::EVENT_KLINE);
            event->setData(kline_data);
            event_engine->putEvent(event);
            
            writeLog(LogLevel::Info, std::string("Published KLINE event: ") + symbol + " " + kline_interval + " close=" + std::to_string(kline_data.close_price));
        }
        
    } catch (const std::exception& e) {
        writeLog(LogLevel::Error, std::string("Exception in OnPush_UpdateKL: ") + e.what());
    }
}

void FutuSpi::OnPush_UpdateRT(const Qot_UpdateRT::Response &stRsp) {
    writeLog(LogLevel::Info, "OnPush_UpdateRT");
}

void FutuSpi::OnPush_UpdateBroker(const Qot_UpdateBroker::Response &stRsp) {
    writeLog(LogLevel::Info, "OnPush_UpdateBroker");
}

void FutuSpi::OnPush_UpdatePriceReminder(const Qot_UpdatePriceReminder::Response &stRsp) {
    writeLog(LogLevel::Info, "OnPush_UpdatePriceReminder");
}

// ========== FTSPI_Trd callbacks ==========

void FutuSpi::OnReply_GetAccList(Futu::u32_t nSerialNo, const Trd_GetAccList::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        acc_list_responses_[nSerialNo] = stRsp;
    }
    writeLog(LogLevel::Info, "OnReply_GetAccList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_UnlockTrade(Futu::u32_t nSerialNo, const Trd_UnlockTrade::Response &stRsp) {
    if (stRsp.rettype() >= 0) {
        writeLog(LogLevel::Info, "Unlock trade successful");
    } else {
        writeLog(LogLevel::Error, std::string("Unlock trade failed: ") + stRsp.retmsg());
    }
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_SubAccPush(Futu::u32_t nSerialNo, const Trd_SubAccPush::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_SubAccPush");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetFunds(Futu::u32_t nSerialNo, const Trd_GetFunds::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        funds_responses_[nSerialNo] = stRsp;
    }
    writeLog(LogLevel::Info, "OnReply_GetFunds");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetPositionList(Futu::u32_t nSerialNo, const Trd_GetPositionList::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        position_responses_[nSerialNo] = stRsp;
    }
    writeLog(LogLevel::Info, "OnReply_GetPositionList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetMaxTrdQtys(Futu::u32_t nSerialNo, const Trd_GetMaxTrdQtys::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetMaxTrdQtys");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetOrderList(Futu::u32_t nSerialNo, const Trd_GetOrderList::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        order_list_responses_[nSerialNo] = stRsp;
    }
    writeLog(LogLevel::Info, "OnReply_GetOrderList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_PlaceOrder(Futu::u32_t nSerialNo, const Trd_PlaceOrder::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        place_order_responses_[nSerialNo] = stRsp;
    }
    
    if (stRsp.rettype() >= 0) {
        writeLog(LogLevel::Info, "Place order successful");
    } else {
        writeLog(LogLevel::Error, std::string("Place order failed: ") + stRsp.retmsg());
    }
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_ModifyOrder(Futu::u32_t nSerialNo, const Trd_ModifyOrder::Response &stRsp) {
    if (stRsp.rettype() >= 0) {
        writeLog(LogLevel::Info, "Modify order successful");
    } else {
        writeLog(LogLevel::Error, std::string("Modify order failed: ") + stRsp.retmsg());
    }
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetOrderFillList(Futu::u32_t nSerialNo, const Trd_GetOrderFillList::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetOrderFillList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetHistoryOrderList(Futu::u32_t nSerialNo, const Trd_GetHistoryOrderList::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetHistoryOrderList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetHistoryOrderFillList(Futu::u32_t nSerialNo, const Trd_GetHistoryOrderFillList::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetHistoryOrderFillList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetMarginRatio(Futu::u32_t nSerialNo, const Trd_GetMarginRatio::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetMarginRatio");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetOrderFee(Futu::u32_t nSerialNo, const Trd_GetOrderFee::Response &stRsp) {
    writeLog(LogLevel::Info, "OnReply_GetOrderFee");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetFlowSummary(Futu::u32_t nSerialNo, const Trd_FlowSummary::Response& stRsp) {
    (void)stRsp;
    writeLog(LogLevel::Info, "OnReply_GetFlowSummary");
    NotifyReply(nSerialNo);
}

// Trade pushes
void FutuSpi::OnPush_UpdateOrder(const Trd_UpdateOrder::Response &stRsp) {
    writeLog(LogLevel::Info, "OnPush_UpdateOrder");
    // TODO: Convert and publish order update events
}

void FutuSpi::OnPush_UpdateOrderFill(const Trd_UpdateOrderFill::Response &stRsp) {
    writeLog(LogLevel::Info, "OnPush_UpdateOrderFill");
    // TODO: Convert and publish trade events
}

void FutuSpi::writeLog(LogLevel level, const std::string& message) {
    this->exchange_->writeLog(level, message);
}

#endif // ENABLE_FUTU
