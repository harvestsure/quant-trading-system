/**
 * Market Scanner 使用示例（简化版本）
 * 
 * 核心设计：直接基于 IExchange 接口，无需额外的数据提供者
 */

#include "scanner/market_scanner.h"
#include "exchange/futu_exchange.h"
#include "utils/logger.h"
#include <memory>
#include <iostream>
#include <thread>

int main() {
    // 1. 创建 Futu 交易所实例
    FutuConfig config;
    config.host = "127.0.0.1";
    config.port = 11111;
    config.market = "HK";
    
    auto futu_exchange = std::make_shared<FutuExchange>(config);
    
    // 2. 连接交易所
    if (!futu_exchange->connect()) {
        LOG_ERROR("Failed to connect to Futu");
        return 1;
    }
    
    // 3. 创建市场扫描器
    MarketScanner scanner;
    
    // 4. 设置交易所（这就是全部！不需要额外的 DataProvider）
    scanner.setExchange(futu_exchange);
    
    // 5. (可选) 设置监控列表
    // 如果不设置，会自动从交易所获取所有股票列表
    // std::vector<std::string> watch_list = {"HK.00700", "HK.00001", "HK.00005"};
    // scanner.setWatchList(watch_list);
    
    // 6. 启动扫描
    scanner.start();
    
    // 7. 获取扫描状态
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        auto status = scanner.getStatus();
        std::cout << "\n=== Scanner Status ===" << std::endl;
        std::cout << "Running: " << (status.running ? "Yes" : "No") << std::endl;
        std::cout << "Watch List: " << status.watch_list_count << " stocks" << std::endl;
        std::cout << "Qualified: " << status.qualified_count << " stocks" << std::endl;
        
        if (!status.qualified_stocks.empty()) {
            std::cout << "Top Results: ";
            for (const auto& code : status.qualified_stocks) {
                std::cout << code << " ";
            }
            std::cout << std::endl;
        }
    }
    
    // 8. 停止扫描
    scanner.stop();
    
    // 9. 断开交易所连接
    futu_exchange->disconnect();
    
    return 0;
}

/**
 * ============================================================================
 * 为什么不需要 IMarketDataProvider？
 * ============================================================================
 * 
 * 问题：为什么要额外的 IMarketDataProvider 接口？
 * 
 * 答案：不需要！我们已经有 IExchange 提供了所有必要的方法：
 * - getMarketStockList()  ← 获取所有可扫描的股票
 * - getBatchSnapshots()   ← 批量获取市场快照
 * 
 * 设计改进：
 * - 移除了冗余的 IMarketDataProvider 接口
 * - MarketScanner 直接依赖 IExchange
 * - 减少了代码复杂性
 * - 更直接、更易维护
 * 
 * ============================================================================
 * 核心优势（KISS原则）
 * ============================================================================
 * 
 * 1. 统一接口
 *    - 所有交易所都实现 IExchange
 *    - 不需要两套接口系统
 *    - 代码更清晰
 * 
 * 2. 直接集成
 *    - 扫描器直接使用交易所对象
 *    - 没有额外的适配层
 *    - 性能更好
 * 
 * 3. 易于扩展
 *    - 添加新交易所只需实现 IExchange
 *    - 自动兼容 MarketScanner
 *    - 无需修改扫描器代码
 * 
 * ============================================================================
 * 工作流程
 * ============================================================================
 * 
 * FutuExchange 实现 IExchange
 *     ↓
 * MarketScanner::setExchange(exchange)
 *     ↓
 * scanOnce()
 *     ↓
 * performScan()
 *     ↓
 * batchFetchMarketData()
 *     ↓
 * exchange->getBatchSnapshots()  ← 直接调用交易所方法
 *     ↓
 * convertSnapshotToScanResult()
 *     ↓
 * calculateScore()
 *     ↓
 * 发送给 StrategyManager
 * 
 * ============================================================================
 * 支持任何 IExchange 实现
 * ============================================================================
 * 
 * // 使用 Bloomberg
 * auto bloomberg = std::make_shared<BloombergExchange>(config);
 * scanner.setExchange(bloomberg);
 * 
 * // 使用 Tiger
 * auto tiger = std::make_shared<TigerExchange>(config);
 * scanner.setExchange(tiger);
 * 
 * // 使用任何实现了 IExchange 的交易所
 * auto any_exchange = ExchangeFactory::createExchange(type, config);
 * scanner.setExchange(any_exchange);
 * 
 * ============================================================================
 */
