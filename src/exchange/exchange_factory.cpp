#include "exchange/exchange_interface.h"
#include "utils/logger.h"
#include <sstream>

// 根据CMake定义的宏来条件编译交易所
#ifdef ENABLE_FUTU
#include "exchange/futu_exchange.h"
#endif

#ifdef ENABLE_IBKR
#include "exchange/ibkr_exchange.h"
#endif

#ifdef ENABLE_BINANCE
#include "exchange/binance_exchange.h"
#endif

std::shared_ptr<IExchange> ExchangeFactory::createExchange(
    ExchangeType type,
    const std::map<std::string, std::string>& config) {
    
    switch (type) {
        case ExchangeType::FUTU: {
// #ifdef ENABLE_FUTU
//             FutuConfig futu_config;
            
//             // 从配置中读取参数
//             if (config.find("host") != config.end()) {
//                 futu_config.host = config.at("host");
//             }
//             if (config.find("port") != config.end()) {
//                 futu_config.port = std::stoi(config.at("port"));
//             }
//             if (config.find("unlock_password") != config.end()) {
//                 futu_config.unlock_password = config.at("unlock_password");
//             }
//             if (config.find("is_simulation") != config.end()) {
//                 futu_config.is_simulation = (config.at("is_simulation") == "true" || config.at("is_simulation") == "1");
//             }
//             if (config.find("market") != config.end()) {
//                 futu_config.market = config.at("market");
//             }
            
//             LOG_INFO("Creating Futu Exchange instance");
//             return std::make_shared<FutuExchange>(futu_config);
// #else
//             LOG_ERROR("Futu exchange is not enabled. Please rebuild with -DENABLE_FUTU=ON");
//             return nullptr;
// #endif
            LOG_ERROR("Futu exchange implementation is not complete");
            return nullptr;
         }
        
        case ExchangeType::IBKR: {
#ifdef ENABLE_IBKR
            // TODO: 配置IBKR参数
            // IBKRConfig ibkr_config;
            // ... 从config读取参数
            LOG_INFO("Creating IBKR Exchange instance");
            // return std::make_shared<IBKRExchange>(ibkr_config);
            LOG_ERROR("IBKR exchange implementation is not complete");
            return nullptr;
#else
            LOG_ERROR("IBKR exchange is not enabled. Please rebuild with -DENABLE_IBKR=ON");
            return nullptr;
#endif
        }
        
        case ExchangeType::BINANCE: {
#ifdef ENABLE_BINANCE
            // TODO: 配置Binance参数
            // BinanceConfig binance_config;
            // ... 从config读取参数
            LOG_INFO("Creating Binance Exchange instance");
            // return std::make_shared<BinanceExchange>(binance_config);
            LOG_ERROR("Binance exchange implementation is not complete");
            return nullptr;
#else
            LOG_ERROR("Binance exchange is not enabled. Please rebuild with -DENABLE_BINANCE=ON");
            return nullptr;
#endif
        }
        
        default: {
            std::stringstream ss;
            ss << "Unknown exchange type: " << static_cast<int>(type);
            LOG_ERROR(ss.str());
            return nullptr;
        }
    }

}
