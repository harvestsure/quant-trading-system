#pragma once

#ifdef ENABLE_FUTU

#include "FTAPI.h"
#include "FTSPI.h"
#include <map>
#include <mutex>
#include <condition_variable>
#include <functional>

// Forward declaration
class FutuExchange;

// Futu SPI 回调处理类
class FutuSpi : public Futu::FTSPI_Conn, public Futu::FTSPI_Qot, public Futu::FTSPI_Trd {
public:
    explicit FutuSpi(FutuExchange* exchange);
    ~FutuSpi();

    // ========== API 生命周期管理 ==========
    bool InitApi(const std::string& host, int port);
    void ReleaseApi();
    bool IsConnected() const;

    // ========== 等待异步响应 ==========
    bool WaitForReply(Futu::u32_t serial_no, int timeout_ms = 5000);
    void NotifyReply(Futu::u32_t serial_no);

    // ========== 交易类请求 Helper 方法 ==========
    Futu::u32_t SendUnlockTrade(const std::string& password);
    Futu::u32_t SendGetAccList();
    Futu::u32_t SendGetFunds(Futu::u64_t acc_id, int trd_env, int trd_market);
    Futu::u32_t SendGetPositionList(Futu::u64_t acc_id, int trd_env, int trd_market);
    Futu::u32_t SendGetOrderList(Futu::u64_t acc_id, int trd_env, int trd_market);
    Futu::u32_t SendPlaceOrder(Futu::u64_t acc_id, int trd_env, int trd_market, 
                               const Qot_Common::Security& security, int order_side, 
                               int order_type, Futu::i64_t quantity, double price);
    Futu::u32_t SendModifyOrder(Futu::u64_t acc_id, int trd_env, Futu::u64_t order_id, 
                                Futu::i64_t quantity, double price);
    Futu::u32_t SendCancelOrder(Futu::u64_t acc_id, int trd_env, Futu::u64_t order_id);

    // ========== 行情类请求 Helper 方法 ==========
    Futu::u32_t SendSubscribeKLine(const Qot_Common::Security& security, int kline_type);
    Futu::u32_t SendGetKLine(const Qot_Common::Security& security, int kline_type, int count);
    Futu::u32_t SendGetHistoryKLine(const Qot_Common::Security& security, int kline_type, int count);
    Futu::u32_t SendGetSecuritySnapshot(const std::vector<Qot_Common::Security>& securities);
    Futu::u32_t SendGetPlateSecurity(const std::string& plate_code);
    Futu::u32_t SendSubscribeTick(const Qot_Common::Security& security);
    Futu::u32_t SendUnsubscribeKLine(const Qot_Common::Security& security);

    // ========== FTSPI_Conn 回调 ==========
    void OnInitConnect(Futu::FTAPI_Conn* pConn, Futu::i64_t nErrCode, const char* strDesc) override;
    void OnDisConnect(Futu::FTAPI_Conn* pConn, Futu::i64_t nErrCode) override;

    // ========== FTSPI_Qot 回调 ==========
    void OnReply_GetGlobalState(Futu::u32_t nSerialNo, const GetGlobalState::Response &stRsp) override;
    void OnReply_Sub(Futu::u32_t nSerialNo, const Qot_Sub::Response &stRsp) override;
    void OnReply_RegQotPush(Futu::u32_t nSerialNo, const Qot_RegQotPush::Response &stRsp) override;
    void OnReply_GetSubInfo(Futu::u32_t nSerialNo, const Qot_GetSubInfo::Response &stRsp) override;
    void OnReply_GetTicker(Futu::u32_t nSerialNo, const Qot_GetTicker::Response &stRsp) override;
    void OnReply_GetBasicQot(Futu::u32_t nSerialNo, const Qot_GetBasicQot::Response &stRsp) override;
    void OnReply_GetOrderBook(Futu::u32_t nSerialNo, const Qot_GetOrderBook::Response &stRsp) override;
    void OnReply_GetKL(Futu::u32_t nSerialNo, const Qot_GetKL::Response &stRsp) override;
    void OnReply_GetRT(Futu::u32_t nSerialNo, const Qot_GetRT::Response &stRsp) override;
    void OnReply_GetBroker(Futu::u32_t nSerialNo, const Qot_GetBroker::Response &stRsp) override;
    void OnReply_RequestRehab(Futu::u32_t nSerialNo, const Qot_RequestRehab::Response &stRsp) override;
    void OnReply_RequestHistoryKL(Futu::u32_t nSerialNo, const Qot_RequestHistoryKL::Response &stRsp) override;
    void OnReply_RequestHistoryKLQuota(Futu::u32_t nSerialNo, const Qot_RequestHistoryKLQuota::Response &stRsp) override;
    void OnReply_GetTradeDate(Futu::u32_t nSerialNo, const Qot_GetTradeDate::Response &stRsp) override;
    void OnReply_GetStaticInfo(Futu::u32_t nSerialNo, const Qot_GetStaticInfo::Response &stRsp) override;
    void OnReply_GetSecuritySnapshot(Futu::u32_t nSerialNo, const Qot_GetSecuritySnapshot::Response &stRsp) override;
    void OnReply_GetPlateSet(Futu::u32_t nSerialNo, const Qot_GetPlateSet::Response &stRsp) override;
    void OnReply_GetPlateSecurity(Futu::u32_t nSerialNo, const Qot_GetPlateSecurity::Response &stRsp) override;
    void OnReply_GetReference(Futu::u32_t nSerialNo, const Qot_GetReference::Response &stRsp) override;
    void OnReply_GetOwnerPlate(Futu::u32_t nSerialNo, const Qot_GetOwnerPlate::Response &stRsp) override;
    void OnReply_GetHoldingChangeList(Futu::u32_t nSerialNo, const Qot_GetHoldingChangeList::Response &stRsp) override;
    void OnReply_GetOptionChain(Futu::u32_t nSerialNo, const Qot_GetOptionChain::Response &stRsp) override;
    void OnReply_GetWarrant(Futu::u32_t nSerialNo, const Qot_GetWarrant::Response &stRsp) override;
    void OnReply_GetCapitalFlow(Futu::u32_t nSerialNo, const Qot_GetCapitalFlow::Response &stRsp) override;
    void OnReply_GetCapitalDistribution(Futu::u32_t nSerialNo, const Qot_GetCapitalDistribution::Response &stRsp) override;
    void OnReply_GetUserSecurity(Futu::u32_t nSerialNo, const Qot_GetUserSecurity::Response &stRsp) override;
    void OnReply_ModifyUserSecurity(Futu::u32_t nSerialNo, const Qot_ModifyUserSecurity::Response &stRsp) override;
    void OnReply_StockFilter(Futu::u32_t nSerialNo, const Qot_StockFilter::Response &stRsp) override;
    void OnReply_GetCodeChange(Futu::u32_t nSerialNo, const Qot_GetCodeChange::Response &stRsp) override;
    void OnReply_GetIpoList(Futu::u32_t nSerialNo, const Qot_GetIpoList::Response &stRsp) override;
    void OnReply_GetFutureInfo(Futu::u32_t nSerialNo, const Qot_GetFutureInfo::Response &stRsp) override;
    void OnReply_RequestTradeDate(Futu::u32_t nSerialNo, const Qot_RequestTradeDate::Response &stRsp) override;
    void OnReply_SetPriceReminder(Futu::u32_t nSerialNo, const Qot_SetPriceReminder::Response &stRsp) override;
    void OnReply_GetPriceReminder(Futu::u32_t nSerialNo, const Qot_GetPriceReminder::Response &stRsp) override;
    void OnReply_GetUserSecurityGroup(Futu::u32_t nSerialNo, const Qot_GetUserSecurityGroup::Response &stRsp) override;
    void OnReply_GetMarketState(Futu::u32_t nSerialNo, const Qot_GetMarketState::Response &stRsp) override;
    void OnReply_GetOptionExpirationDate(Futu::u32_t nSerialNo, const Qot_GetOptionExpirationDate::Response &stRsp) override;

    // 行情推送
    void OnPush_Notify(const Notify::Response &stRsp) override;
    void OnPush_UpdateBasicQot(const Qot_UpdateBasicQot::Response &stRsp) override;
    void OnPush_UpdateOrderBook(const Qot_UpdateOrderBook::Response &stRsp) override;
    void OnPush_UpdateTicker(const Qot_UpdateTicker::Response &stRsp) override;
    void OnPush_UpdateKL(const Qot_UpdateKL::Response &stRsp) override;
    void OnPush_UpdateRT(const Qot_UpdateRT::Response &stRsp) override;
    void OnPush_UpdateBroker(const Qot_UpdateBroker::Response &stRsp) override;
    void OnPush_UpdatePriceReminder(const Qot_UpdatePriceReminder::Response &stRsp) override;

    // ========== FTSPI_Trd 回调 ==========
    void OnReply_GetAccList(Futu::u32_t nSerialNo, const Trd_GetAccList::Response &stRsp) override;
    void OnReply_UnlockTrade(Futu::u32_t nSerialNo, const Trd_UnlockTrade::Response &stRsp) override;
    void OnReply_SubAccPush(Futu::u32_t nSerialNo, const Trd_SubAccPush::Response &stRsp) override;
    void OnReply_GetFunds(Futu::u32_t nSerialNo, const Trd_GetFunds::Response &stRsp) override;
    void OnReply_GetPositionList(Futu::u32_t nSerialNo, const Trd_GetPositionList::Response &stRsp) override;
    void OnReply_GetMaxTrdQtys(Futu::u32_t nSerialNo, const Trd_GetMaxTrdQtys::Response &stRsp) override;
    void OnReply_GetOrderList(Futu::u32_t nSerialNo, const Trd_GetOrderList::Response &stRsp) override;
    void OnReply_PlaceOrder(Futu::u32_t nSerialNo, const Trd_PlaceOrder::Response &stRsp) override;
    void OnReply_ModifyOrder(Futu::u32_t nSerialNo, const Trd_ModifyOrder::Response &stRsp) override;
    void OnReply_GetOrderFillList(Futu::u32_t nSerialNo, const Trd_GetOrderFillList::Response &stRsp) override;
    void OnReply_GetHistoryOrderList(Futu::u32_t nSerialNo, const Trd_GetHistoryOrderList::Response &stRsp) override;
    void OnReply_GetHistoryOrderFillList(Futu::u32_t nSerialNo, const Trd_GetHistoryOrderFillList::Response &stRsp) override;
    void OnReply_GetMarginRatio(Futu::u32_t nSerialNo, const Trd_GetMarginRatio::Response &stRsp) override;
    void OnReply_GetOrderFee(Futu::u32_t nSerialNo, const Trd_GetOrderFee::Response &stRsp) override;
    void OnReply_GetFlowSummary(Futu::u32_t nSerialNo, const Trd_FlowSummary::Response& stRsp) override;

    // 交易推送
    void OnPush_UpdateOrder(const Trd_UpdateOrder::Response &stRsp) override;
    void OnPush_UpdateOrderFill(const Trd_UpdateOrderFill::Response &stRsp) override;

    // 存储响应数据
    std::map<Futu::u32_t, Trd_GetAccList::Response> acc_list_responses_;
    std::map<Futu::u32_t, Trd_GetFunds::Response> funds_responses_;
    std::map<Futu::u32_t, Trd_GetPositionList::Response> position_responses_;
    std::map<Futu::u32_t, Trd_GetOrderList::Response> order_list_responses_;
    std::map<Futu::u32_t, Trd_PlaceOrder::Response> place_order_responses_;
    std::map<Futu::u32_t, Qot_GetSecuritySnapshot::Response> snapshot_responses_;
    std::map<Futu::u32_t, Qot_GetKL::Response> kline_responses_;
    std::map<Futu::u32_t, Qot_RequestHistoryKL::Response> history_kline_responses_;
    std::map<Futu::u32_t, Qot_GetPlateSecurity::Response> plate_security_responses_;
    
    friend class FutuExchange;  // 允许 FutuExchange 访问 mutex_ 和响应数据
    
private:
    FutuExchange* exchange_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::map<Futu::u32_t, bool> reply_flags_;
    
    // API 实例管理
    Futu::FTAPI_Qot* qot_api_ = nullptr;
    Futu::FTAPI_Trd* trd_api_ = nullptr;
	bool is_qot_connected_ = false;
	bool is_trd_connected_ = false;
    bool api_initialized_ = false;
    std::string host_;
    int port_ = 11111;
};

#endif // ENABLE_FUTU
