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

// ========== FTSPI_Conn 回调 ==========

void FutuSpi::OnInitConnect(Futu::FTAPI_Conn* pConn, Futu::i64_t nErrCode, const char* strDesc) {
    if (nErrCode == 0) {
        LOG_INFO(std::string("FTAPI connected successfully: ") + strDesc);
    } else {
        LOG_ERROR(std::string("FTAPI connection failed: code=") + std::to_string(nErrCode) + ", desc=" + strDesc);
    }
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
