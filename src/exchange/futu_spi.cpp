#ifdef ENABLE_FUTU

#include "futu_spi.h"
#include "exchange/futu_exchange.h"
#include "utils/logger.h"
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

// ========== API 生命周期管理 ==========

bool FutuSpi::InitApi(const std::string& host, int port) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (api_initialized_) {
            LOG_WARNING("API already initialized");
            return true;
        }
        
        host_ = host;
        port_ = port;
    }  // 释放锁，因为 WaitForReply 会再次获取
    
    try {
        // 初始化 FTAPI
        Futu::FTAPI::Init();
        
        // 创建行情API
        qot_api_ = Futu::FTAPI::CreateQotApi();
        if (qot_api_ == nullptr) {
            LOG_ERROR("Failed to create Qot API");
            return false;
        }
        
        // 设置客户端信息
        qot_api_->SetClientInfo("QUANT_TRADING_SYSTEM", 1);
        
        // 注册回调
        qot_api_->RegisterConnSpi(this);
        qot_api_->RegisterQotSpi(this);
        
        // 初始化连接
        bool ret = qot_api_->InitConnect(host.c_str(), port, false);
        /*if (!ret) {
            LOG_ERROR("Failed to initialize Qot API connection");
            Futu::FTAPI::ReleaseQotApi(qot_api_);
            qot_api_ = nullptr;
            return false;
        }*/

        WaitForReply(0, 5000); // 等待连接初始化完成，serial_no 0 用于表示连接初始化

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!is_qot_connected_) {
                LOG_ERROR("Qot API connection failed");
                Futu::FTAPI::ReleaseQotApi(qot_api_);
                qot_api_ = nullptr;
                return false;
            }
        }
        
        // 创建交易API
        trd_api_ = Futu::FTAPI::CreateTrdApi();
        if (trd_api_ == nullptr) {
            LOG_ERROR("Failed to create Trd API");
            qot_api_->Close();
            Futu::FTAPI::ReleaseQotApi(qot_api_);
            qot_api_ = nullptr;
            return false;
        }
        
        // 设置客户端信息
        trd_api_->SetClientInfo("QUANT_TRADING_SYSTEM", 1);
        
        // 注册回调
        trd_api_->RegisterConnSpi(this);
        trd_api_->RegisterTrdSpi(this);
        
        // 初始化连接
        ret = trd_api_->InitConnect(host.c_str(), port, false);
        /* if (!ret) {
            LOG_ERROR("Failed to initialize Trd API connection");
            qot_api_->Close();
            Futu::FTAPI::ReleaseQotApi(qot_api_);
            Futu::FTAPI::ReleaseTrdApi(trd_api_);
            qot_api_ = nullptr;
            trd_api_ = nullptr;
            return false;
        }*/

        WaitForReply(0, 5000); // 等待连接初始化完成，serial_no 0 用于表示连接初始化

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!is_trd_connected_) {
                LOG_ERROR("Trd API connection failed");
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
        
        LOG_INFO(std::string("FTAPI initialized successfully at ") + host + ":" + std::to_string(port));
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during API initialization: ") + e.what());
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
        LOG_INFO("FTAPI released successfully");
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during API release: ") + e.what());
    }
}

bool FutuSpi::IsConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return api_initialized_ && qot_api_ != nullptr && trd_api_ != nullptr;
}

// ========== 交易类请求 Helper 方法 ==========

Futu::u32_t FutuSpi::SendUnlockTrade(const std::string& password) {
    if (trd_api_ == nullptr) {
        LOG_ERROR("Trd API not initialized");
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
            LOG_ERROR("Failed to send unlock trade request");
            return 0;
        }
        
        LOG_INFO(std::string("Sent unlock trade request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during send unlock trade: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetAccList() {
    if (trd_api_ == nullptr) {
        LOG_ERROR("Trd API not initialized");
        return 0;
    }
    
    try {
        Trd_GetAccList::Request req;
        auto* c2s = req.mutable_c2s();
        c2s->set_userid(0);  // 0 表示当前连接对应的用户
        
        Futu::u32_t serial_no = trd_api_->GetAccList(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send get account list request");
            return 0;
        }
        
        LOG_INFO(std::string("Sent get account list request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during send get account list: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetFunds(Futu::u64_t acc_id, int trd_env, int trd_market) {
    if (trd_api_ == nullptr) {
        LOG_ERROR("Trd API not initialized");
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
            LOG_ERROR("Failed to send get funds request");
            return 0;
        }
        
        LOG_INFO(std::string("Sent get funds request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during send get funds: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetPositionList(Futu::u64_t acc_id, int trd_env, int trd_market) {
    if (trd_api_ == nullptr) {
        LOG_ERROR("Trd API not initialized");
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
            LOG_ERROR("Failed to send get position list request");
            return 0;
        }
        
        LOG_INFO(std::string("Sent get position list request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during send get position list: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetOrderList(Futu::u64_t acc_id, int trd_env, int trd_market) {
    if (trd_api_ == nullptr) {
        LOG_ERROR("Trd API not initialized");
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
            LOG_ERROR("Failed to send get order list request");
            return 0;
        }
        
        LOG_INFO(std::string("Sent get order list request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during send get order list: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendPlaceOrder(Futu::u64_t acc_id, int trd_env, int trd_market,
                                     const Qot_Common::Security& security, int order_side,
                                     int order_type, Futu::i64_t quantity, double price) {
    if (trd_api_ == nullptr) {
        LOG_ERROR("Trd API not initialized");
        return 0;
    }
    
    try {
        Trd_PlaceOrder::Request req;
        auto* c2s = req.mutable_c2s();
        auto* header = c2s->mutable_header();
        header->set_trdenv(trd_env);
        header->set_accid(acc_id);
        header->set_trdmarket(trd_market);
        
        // 设置订单参数（直接在 C2S 中设置）
        c2s->set_trdside(order_side);
        c2s->set_ordertype(order_type);
        c2s->set_code(security.code());
        c2s->set_qty((double)quantity);
        if (price > 0) {
            c2s->set_price(price);
        }
        
        Futu::u32_t serial_no = trd_api_->PlaceOrder(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send place order request");
            return 0;
        }
        
        LOG_INFO(std::string("Sent place order request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during send place order: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendModifyOrder(Futu::u64_t acc_id, int trd_env, Futu::u64_t order_id,
                                      Futu::i64_t quantity, double price) {
    if (trd_api_ == nullptr) {
        LOG_ERROR("Trd API not initialized");
        return 0;
    }
    
    try {
        Trd_ModifyOrder::Request req;
        auto* c2s = req.mutable_c2s();
        auto* header = c2s->mutable_header();
        header->set_trdenv(trd_env);
        header->set_accid(acc_id);
        header->set_trdmarket(Trd_Common::TrdMarket_HK);  // 默认使用HK市场
        
        c2s->set_orderid(order_id);
        c2s->set_qty(quantity);
        c2s->set_price(price);
        
        Futu::u32_t serial_no = trd_api_->ModifyOrder(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send modify order request");
            return 0;
        }
        
        LOG_INFO(std::string("Sent modify order request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during send modify order: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendCancelOrder(Futu::u64_t acc_id, int trd_env, Futu::u64_t order_id) {
    if (trd_api_ == nullptr) {
        LOG_ERROR("Trd API not initialized");
        return 0;
    }
    
    try {
        // 使用 ModifyOrder 来取消订单（通过设置 Order 的 Status 为 Cancelled）
        Trd_ModifyOrder::Request req;
        auto* c2s = req.mutable_c2s();
        auto* header = c2s->mutable_header();
        header->set_trdenv(trd_env);
        header->set_accid(acc_id);
        header->set_trdmarket(Trd_Common::TrdMarket_HK);
        
        c2s->set_orderid(order_id);
        
        Futu::u32_t serial_no = trd_api_->ModifyOrder(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send cancel order request");
            return 0;
        }
        
        LOG_INFO(std::string("Sent cancel order request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during send cancel order: ") + e.what());
        return 0;
    }
}

// ========== 行情类请求 Helper 方法 ==========

Futu::u32_t FutuSpi::SendSubscribeKLine(const Qot_Common::Security& security, int kline_type) {
    if (qot_api_ == nullptr) {
        LOG_ERROR("Qot API not initialized");
        return 0;
    }
    
    try {
        Qot_Sub::Request req;
        auto* c2s = req.mutable_c2s();
        
        // 添加要订阅的股票
        auto* security_item = c2s->add_securitylist();
        *security_item = security;
        
        // 添加订阅类型 - K线
        // 直接使用整数值 1,2,3... 或者查看Qot_Common的具体值
        c2s->add_subtypelist(1 + kline_type);  // SubType_K_1Min = 1, 1Min=0, 3Min=1 等等
        
        // 设置为订阅
        c2s->set_issuborunsub(true);
        c2s->set_isregorunregpush(true);
        
        Futu::u32_t serial_no = qot_api_->Sub(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send subscribe KLine request");
            return 0;
        }
        
        LOG_INFO(std::string("Sent subscribe KLine request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during send subscribe KLine: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetKLine(const Qot_Common::Security& security, int kline_type, int count) {
    if (qot_api_ == nullptr) {
        LOG_ERROR("Qot API not initialized");
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
            LOG_ERROR("Failed to send get KLine request");
            return 0;
        }
        
        LOG_INFO(std::string("Sent get KLine request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during send get KLine: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetHistoryKLine(const Qot_Common::Security& security, int kline_type, int count) {
    if (qot_api_ == nullptr) {
        LOG_ERROR("Qot API not initialized");
        return 0;
    }
    
    try {
        Qot_RequestHistoryKL::Request req;
        auto* c2s = req.mutable_c2s();
        *c2s->mutable_security() = security;
        c2s->set_kltype(kline_type);
        c2s->set_maxackklnum(count);  // 使用 maxackklnum 而不是 reqnum
        
        Futu::u32_t serial_no = qot_api_->RequestHistoryKL(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send request history KLine request");
            return 0;
        }
        
        LOG_INFO(std::string("Sent request history KLine, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during send request history KLine: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetSecuritySnapshot(const std::vector<Qot_Common::Security>& securities) {
    if (qot_api_ == nullptr) {
        LOG_ERROR("Qot API not initialized");
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
            LOG_ERROR("Failed to send get security snapshot request");
            return 0;
        }
        
        LOG_INFO(std::string("Sent get security snapshot request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during send get security snapshot: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendGetPlateSecurity(const std::string& plate_code) {
    if (qot_api_ == nullptr) {
        LOG_ERROR("Qot API not initialized");
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
            LOG_ERROR("Failed to send get plate security request");
            return 0;
        }
        
        LOG_INFO(std::string("Sent get plate security request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during send get plate security: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendSubscribeTick(const Qot_Common::Security& security) {
    if (qot_api_ == nullptr) {
        LOG_ERROR("Qot API not initialized");
        return 0;
    }
    
    try {
        Qot_Sub::Request req;
        auto* c2s = req.mutable_c2s();
        
        // 添加要订阅的股票
        auto* security_item = c2s->add_securitylist();
        *security_item = security;
        
        // 添加订阅类型 - 基础报价和Ticker
        c2s->add_subtypelist(Qot_Common::SubType_Basic);
        c2s->add_subtypelist(Qot_Common::SubType_Ticker);
        
        // 设置为订阅
        c2s->set_issuborunsub(true);
        c2s->set_isregorunregpush(true);
        
        Futu::u32_t serial_no = qot_api_->Sub(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send subscribe Tick request");
            return 0;
        }
        
        LOG_INFO(std::string("Sent subscribe Tick request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during send subscribe Tick: ") + e.what());
        return 0;
    }
}

Futu::u32_t FutuSpi::SendUnsubscribeKLine(const Qot_Common::Security& security) {
    if (qot_api_ == nullptr) {
        LOG_ERROR("Qot API not initialized");
        return 0;
    }
    
    try {
        Qot_Sub::Request req;
        auto* c2s = req.mutable_c2s();
        
        // 添加要取消订阅的股票
        auto* security_item = c2s->add_securitylist();
        *security_item = security;
        
        // 设置为取消订阅
        c2s->set_issuborunsub(false);
        
        Futu::u32_t serial_no = qot_api_->Sub(req);
        if (serial_no == 0) {
            LOG_ERROR("Failed to send unsubscribe KLine request");
            return 0;
        }
        
        LOG_INFO(std::string("Sent unsubscribe KLine request, serial_no=") + std::to_string(serial_no));
        return serial_no;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during send unsubscribe KLine: ") + e.what());
        return 0;
    }
}

// ========== FTSPI_Conn 回调 ==========

void FutuSpi::OnInitConnect(Futu::FTAPI_Conn* pConn, Futu::i64_t nErrCode, const char* strDesc) {
    if (nErrCode == 0) {
        LOG_INFO(std::string("FTAPI connected successfully: ") + strDesc);
    } else {
        LOG_ERROR(std::string("FTAPI connection failed: code=") + std::to_string(nErrCode) + ", desc=" + strDesc);
    }

    if (pConn == qot_api_) {
		is_qot_connected_ = (nErrCode == 0);
    } else if (pConn == trd_api_) {
		is_trd_connected_ = (nErrCode == 0);
	}

	NotifyReply(0); // 使用 serial_no 0 来表示连接初始化完成    
}

void FutuSpi::OnDisConnect(Futu::FTAPI_Conn* pConn, Futu::i64_t nErrCode) {
    LOG_WARNING(std::string("FTAPI disconnected: code=") + std::to_string(nErrCode));
}

// ========== FTSPI_Qot 回调 ==========

void FutuSpi::OnReply_GetGlobalState(Futu::u32_t nSerialNo, const GetGlobalState::Response &stRsp) {
    LOG_INFO("OnReply_GetGlobalState");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_Sub(Futu::u32_t nSerialNo, const Qot_Sub::Response &stRsp) {
    if (stRsp.rettype() >= 0) {
        LOG_INFO("Subscribe successful");
    } else {
        LOG_ERROR(std::string("Subscribe failed: ") + stRsp.retmsg());
    }
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_RegQotPush(Futu::u32_t nSerialNo, const Qot_RegQotPush::Response &stRsp) {
    LOG_INFO("OnReply_RegQotPush");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetSubInfo(Futu::u32_t nSerialNo, const Qot_GetSubInfo::Response &stRsp) {
    LOG_INFO("OnReply_GetSubInfo");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetTicker(Futu::u32_t nSerialNo, const Qot_GetTicker::Response &stRsp) {
    LOG_INFO("OnReply_GetTicker");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetBasicQot(Futu::u32_t nSerialNo, const Qot_GetBasicQot::Response &stRsp) {
    LOG_INFO("OnReply_GetBasicQot");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetOrderBook(Futu::u32_t nSerialNo, const Qot_GetOrderBook::Response &stRsp) {
    LOG_INFO("OnReply_GetOrderBook");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetKL(Futu::u32_t nSerialNo, const Qot_GetKL::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        kline_responses_[nSerialNo] = stRsp;
    }
    LOG_INFO("OnReply_GetKL");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetRT(Futu::u32_t nSerialNo, const Qot_GetRT::Response &stRsp) {
    LOG_INFO("OnReply_GetRT");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetBroker(Futu::u32_t nSerialNo, const Qot_GetBroker::Response &stRsp) {
    LOG_INFO("OnReply_GetBroker");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_RequestRehab(Futu::u32_t nSerialNo, const Qot_RequestRehab::Response &stRsp) {
    LOG_INFO("OnReply_RequestRehab");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_RequestHistoryKL(Futu::u32_t nSerialNo, const Qot_RequestHistoryKL::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        history_kline_responses_[nSerialNo] = stRsp;
    }
    LOG_INFO("OnReply_RequestHistoryKL");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_RequestHistoryKLQuota(Futu::u32_t nSerialNo, const Qot_RequestHistoryKLQuota::Response &stRsp) {
    LOG_INFO("OnReply_RequestHistoryKLQuota");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetTradeDate(Futu::u32_t nSerialNo, const Qot_GetTradeDate::Response &stRsp) {
    LOG_INFO("OnReply_GetTradeDate");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetStaticInfo(Futu::u32_t nSerialNo, const Qot_GetStaticInfo::Response &stRsp) {
    LOG_INFO("OnReply_GetStaticInfo");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetSecuritySnapshot(Futu::u32_t nSerialNo, const Qot_GetSecuritySnapshot::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_responses_[nSerialNo] = stRsp;
    }
    LOG_INFO("OnReply_GetSecuritySnapshot");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetPlateSet(Futu::u32_t nSerialNo, const Qot_GetPlateSet::Response &stRsp) {
    LOG_INFO("OnReply_GetPlateSet");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetPlateSecurity(Futu::u32_t nSerialNo, const Qot_GetPlateSecurity::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        plate_security_responses_[nSerialNo] = stRsp;
    }
    LOG_INFO("OnReply_GetPlateSecurity");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetReference(Futu::u32_t nSerialNo, const Qot_GetReference::Response &stRsp) {
    LOG_INFO("OnReply_GetReference");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetOwnerPlate(Futu::u32_t nSerialNo, const Qot_GetOwnerPlate::Response &stRsp) {
    LOG_INFO("OnReply_GetOwnerPlate");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetHoldingChangeList(Futu::u32_t nSerialNo, const Qot_GetHoldingChangeList::Response &stRsp) {
    LOG_INFO("OnReply_GetHoldingChangeList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetOptionChain(Futu::u32_t nSerialNo, const Qot_GetOptionChain::Response &stRsp) {
    LOG_INFO("OnReply_GetOptionChain");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetWarrant(Futu::u32_t nSerialNo, const Qot_GetWarrant::Response &stRsp) {
    LOG_INFO("OnReply_GetWarrant");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetCapitalFlow(Futu::u32_t nSerialNo, const Qot_GetCapitalFlow::Response &stRsp) {
    LOG_INFO("OnReply_GetCapitalFlow");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetCapitalDistribution(Futu::u32_t nSerialNo, const Qot_GetCapitalDistribution::Response &stRsp) {
    LOG_INFO("OnReply_GetCapitalDistribution");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetUserSecurity(Futu::u32_t nSerialNo, const Qot_GetUserSecurity::Response &stRsp) {
    LOG_INFO("OnReply_GetUserSecurity");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_ModifyUserSecurity(Futu::u32_t nSerialNo, const Qot_ModifyUserSecurity::Response &stRsp) {
    LOG_INFO("OnReply_ModifyUserSecurity");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_StockFilter(Futu::u32_t nSerialNo, const Qot_StockFilter::Response &stRsp) {
    LOG_INFO("OnReply_StockFilter");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetCodeChange(Futu::u32_t nSerialNo, const Qot_GetCodeChange::Response &stRsp) {
    LOG_INFO("OnReply_GetCodeChange");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetIpoList(Futu::u32_t nSerialNo, const Qot_GetIpoList::Response &stRsp) {
    LOG_INFO("OnReply_GetIpoList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetFutureInfo(Futu::u32_t nSerialNo, const Qot_GetFutureInfo::Response &stRsp) {
    LOG_INFO("OnReply_GetFutureInfo");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_RequestTradeDate(Futu::u32_t nSerialNo, const Qot_RequestTradeDate::Response &stRsp) {
    LOG_INFO("OnReply_RequestTradeDate");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_SetPriceReminder(Futu::u32_t nSerialNo, const Qot_SetPriceReminder::Response &stRsp) {
    LOG_INFO("OnReply_SetPriceReminder");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetPriceReminder(Futu::u32_t nSerialNo, const Qot_GetPriceReminder::Response &stRsp) {
    LOG_INFO("OnReply_GetPriceReminder");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetUserSecurityGroup(Futu::u32_t nSerialNo, const Qot_GetUserSecurityGroup::Response &stRsp) {
    LOG_INFO("OnReply_GetUserSecurityGroup");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetMarketState(Futu::u32_t nSerialNo, const Qot_GetMarketState::Response &stRsp) {
    LOG_INFO("OnReply_GetMarketState");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetOptionExpirationDate(Futu::u32_t nSerialNo, const Qot_GetOptionExpirationDate::Response &stRsp) {
    LOG_INFO("OnReply_GetOptionExpirationDate");
    NotifyReply(nSerialNo);
}

// 行情推送
void FutuSpi::OnPush_Notify(const Notify::Response &stRsp) {
    LOG_INFO("OnPush_Notify");
}

void FutuSpi::OnPush_UpdateBasicQot(const Qot_UpdateBasicQot::Response &stRsp) {
    LOG_INFO("OnPush_UpdateBasicQot");
    // TODO: 转换并发布 Tick 事件
}

void FutuSpi::OnPush_UpdateOrderBook(const Qot_UpdateOrderBook::Response &stRsp) {
    LOG_INFO("OnPush_UpdateOrderBook");
}

void FutuSpi::OnPush_UpdateTicker(const Qot_UpdateTicker::Response &stRsp) {
    LOG_INFO("OnPush_UpdateTicker");
}

void FutuSpi::OnPush_UpdateKL(const Qot_UpdateKL::Response &stRsp) {
    LOG_INFO("OnPush_UpdateKL");
    // TODO: 转换并发布 KLine 事件
}

void FutuSpi::OnPush_UpdateRT(const Qot_UpdateRT::Response &stRsp) {
    LOG_INFO("OnPush_UpdateRT");
}

void FutuSpi::OnPush_UpdateBroker(const Qot_UpdateBroker::Response &stRsp) {
    LOG_INFO("OnPush_UpdateBroker");
}

void FutuSpi::OnPush_UpdatePriceReminder(const Qot_UpdatePriceReminder::Response &stRsp) {
    LOG_INFO("OnPush_UpdatePriceReminder");
}

// ========== FTSPI_Trd 回调 ==========

void FutuSpi::OnReply_GetAccList(Futu::u32_t nSerialNo, const Trd_GetAccList::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        acc_list_responses_[nSerialNo] = stRsp;
    }
    LOG_INFO("OnReply_GetAccList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_UnlockTrade(Futu::u32_t nSerialNo, const Trd_UnlockTrade::Response &stRsp) {
    if (stRsp.rettype() >= 0) {
        LOG_INFO("Unlock trade successful");
    } else {
        LOG_ERROR(std::string("Unlock trade failed: ") + stRsp.retmsg());
    }
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_SubAccPush(Futu::u32_t nSerialNo, const Trd_SubAccPush::Response &stRsp) {
    LOG_INFO("OnReply_SubAccPush");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetFunds(Futu::u32_t nSerialNo, const Trd_GetFunds::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        funds_responses_[nSerialNo] = stRsp;
    }
    LOG_INFO("OnReply_GetFunds");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetPositionList(Futu::u32_t nSerialNo, const Trd_GetPositionList::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        position_responses_[nSerialNo] = stRsp;
    }
    LOG_INFO("OnReply_GetPositionList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetMaxTrdQtys(Futu::u32_t nSerialNo, const Trd_GetMaxTrdQtys::Response &stRsp) {
    LOG_INFO("OnReply_GetMaxTrdQtys");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetOrderList(Futu::u32_t nSerialNo, const Trd_GetOrderList::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        order_list_responses_[nSerialNo] = stRsp;
    }
    LOG_INFO("OnReply_GetOrderList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_PlaceOrder(Futu::u32_t nSerialNo, const Trd_PlaceOrder::Response &stRsp) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        place_order_responses_[nSerialNo] = stRsp;
    }
    
    if (stRsp.rettype() >= 0) {
        LOG_INFO("Place order successful");
    } else {
        LOG_ERROR(std::string("Place order failed: ") + stRsp.retmsg());
    }
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_ModifyOrder(Futu::u32_t nSerialNo, const Trd_ModifyOrder::Response &stRsp) {
    if (stRsp.rettype() >= 0) {
        LOG_INFO("Modify order successful");
    } else {
        LOG_ERROR(std::string("Modify order failed: ") + stRsp.retmsg());
    }
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetOrderFillList(Futu::u32_t nSerialNo, const Trd_GetOrderFillList::Response &stRsp) {
    LOG_INFO("OnReply_GetOrderFillList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetHistoryOrderList(Futu::u32_t nSerialNo, const Trd_GetHistoryOrderList::Response &stRsp) {
    LOG_INFO("OnReply_GetHistoryOrderList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetHistoryOrderFillList(Futu::u32_t nSerialNo, const Trd_GetHistoryOrderFillList::Response &stRsp) {
    LOG_INFO("OnReply_GetHistoryOrderFillList");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetMarginRatio(Futu::u32_t nSerialNo, const Trd_GetMarginRatio::Response &stRsp) {
    LOG_INFO("OnReply_GetMarginRatio");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetOrderFee(Futu::u32_t nSerialNo, const Trd_GetOrderFee::Response &stRsp) {
    LOG_INFO("OnReply_GetOrderFee");
    NotifyReply(nSerialNo);
}

void FutuSpi::OnReply_GetFlowSummary(Futu::u32_t nSerialNo, const Trd_FlowSummary::Response& stRsp) {
    (void)stRsp;
    LOG_INFO("OnReply_GetFlowSummary");
    NotifyReply(nSerialNo);
}

// 交易推送
void FutuSpi::OnPush_UpdateOrder(const Trd_UpdateOrder::Response &stRsp) {
    LOG_INFO("OnPush_UpdateOrder");
    // TODO: 转换并发布订单更新事件
}

void FutuSpi::OnPush_UpdateOrderFill(const Trd_UpdateOrderFill::Response &stRsp) {
    LOG_INFO("OnPush_UpdateOrderFill");
    // TODO: 转换并发布成交事件
}

#endif // ENABLE_FUTU
