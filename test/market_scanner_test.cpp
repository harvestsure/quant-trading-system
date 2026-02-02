/**
 * Market Scanner 单元测试示例
 * 使用Mock交易所实现
 */

#include "scanner/market_scanner.h"
#include "exchange/exchange_interface.h"
#include <memory>
#include <thread>
#include <chrono>
#include <cassert>
#include <iostream>

// Mock 交易所实现 - 用于测试
class MockExchange : public IExchange {
public:
    MockExchange() : call_count_(0), connected_(true) {}
    
    bool connect() override { connected_ = true; return true; }
    bool disconnect() override { connected_ = false; return true; }
    bool isConnected() const override { return connected_; }
    ExchangeType getType() const override { return ExchangeType::FUTU; }
    std::string getName() const override { return "Mock Exchange"; }
    
    AccountInfo getAccountInfo() override {
        return AccountInfo{"ACC001", 1000000, 500000, 500000, 500000, 0, "HKD"};
    }
    
    std::vector<ExchangePosition> getPositions() override { return {}; }
    double getAvailableFunds() override { return 500000; }
    
    std::string placeOrder(const std::string&, const std::string&, int, const std::string&, double) override {
        return "ORDER001";
    }
    
    bool cancelOrder(const std::string&) override { return true; }
    bool modifyOrder(const std::string&, int, double) override { return true; }
    OrderData getOrderStatus(const std::string&) override { return OrderData{}; }
    std::vector<OrderData> getOrderHistory(int) override { return {}; }
    
    bool subscribeKLine(const std::string&, const std::string&) override { return true; }
    bool unsubscribeKLine(const std::string&) override { return true; }
    bool subscribeTick(const std::string&) override { return true; }
    bool unsubscribeTick(const std::string&) override { return true; }
    
    std::vector<KlineData> getHistoryKLine(const std::string&, const std::string&, int) override {
        return {};
    }
    
    Snapshot getSnapshot(const std::string& symbol) override {
        Snapshot s;
        s.code = symbol;
        s.name = "Test_" + symbol;
        s.last_price = 100.0;
        s.change_rate = 0.03;
        s.volume = 10000000;
        s.turnover_rate = 0.05;
        return s;
    }
    
    std::vector<std::string> getMarketStockList(const std::string&) override {
        return {
            "HK.00700", "HK.00001", "HK.00005", "HK.00011", "HK.00027",
            "HK.00066", "HK.00175", "HK.00267", "HK.00688", "HK.00857"
        };
    }
    
    std::map<std::string, Snapshot> getBatchSnapshots(const std::vector<std::string>& codes) override {
        call_count_++;
        std::map<std::string, Snapshot> results;
        
        for (size_t i = 0; i < codes.size(); ++i) {
            Snapshot s;
            s.code = codes[i];
            s.name = "Test_" + codes[i];
            s.last_price = 100.0 + i;
            s.change_rate = 0.02 + (i % 5) * 0.01;
            s.volume = 10000000 + i * 1000000;
            s.turnover_rate = 0.02 + (i % 10) * 0.01;
            results[codes[i]] = s;
        }
        
        return results;
    }
    
    int getCallCount() const { return call_count_; }
    void resetCallCount() { call_count_ = 0; }
    
private:
    int call_count_;
    bool connected_;
};

#define TEST_ASSERT(condition, message) \
    if (!(condition)) { \
        std::cerr << "TEST FAILED: " << message << std::endl; \
        return false; \
    }

bool test_set_exchange() {
    std::cout << "Running: test_set_exchange..." << std::endl;
    
    auto scanner = std::make_unique<MarketScanner>();
    auto mock_exchange = std::make_shared<MockExchange>();
    
    TEST_ASSERT(!scanner->isRunning(), "Scanner should not be running initially");
    
    scanner->setExchange(mock_exchange);
    
    std::cout << "PASSED: test_set_exchange" << std::endl;
    return true;
}

bool test_start_stop() {
    std::cout << "Running: test_start_stop..." << std::endl;
    
    auto scanner = std::make_unique<MarketScanner>();
    auto mock_exchange = std::make_shared<MockExchange>();
    scanner->setExchange(mock_exchange);
    
    scanner->start();
    TEST_ASSERT(scanner->isRunning(), "Scanner should be running after start()");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    scanner->stop();
    TEST_ASSERT(!scanner->isRunning(), "Scanner should not be running after stop()");
    
    std::cout << "PASSED: test_start_stop" << std::endl;
    return true;
}

bool test_watch_list() {
    std::cout << "Running: test_watch_list..." << std::endl;
    
    auto scanner = std::make_unique<MarketScanner>();
    std::vector<std::string> watch_list = {
        "HK.00700", "HK.00001", "HK.00005"
    };
    
    scanner->setWatchList(watch_list);
    auto status = scanner->getStatus();
    TEST_ASSERT(status.watch_list_count == 3, "Watch list should contain 3 stocks");
    
    std::cout << "PASSED: test_watch_list" << std::endl;
    return true;
}

bool test_get_status() {
    std::cout << "Running: test_get_status..." << std::endl;
    
    auto scanner = std::make_unique<MarketScanner>();
    auto mock_exchange = std::make_shared<MockExchange>();
    scanner->setExchange(mock_exchange);
    scanner->start();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto status = scanner->getStatus();
    TEST_ASSERT(status.running, "Status should show scanner as running");
    
    scanner->stop();
    
    std::cout << "PASSED: test_get_status" << std::endl;
    return true;
}

bool test_immediate_scan() {
    std::cout << "Running: test_immediate_scan..." << std::endl;
    
    auto scanner = std::make_unique<MarketScanner>();
    auto mock_exchange = std::make_shared<MockExchange>();
    std::vector<std::string> watch_list = {"HK.00700", "HK.00001"};
    
    scanner->setWatchList(watch_list);
    scanner->setExchange(mock_exchange);
    
    mock_exchange->resetCallCount();
    scanner->scanOnce();
    
    TEST_ASSERT(mock_exchange->getCallCount() > 0, "Exchange should be called");
    
    std::cout << "PASSED: test_immediate_scan" << std::endl;
    return true;
}

int main() {
    std::cout << "========== Market Scanner Unit Tests ==========" << std::endl;
    std::cout << std::endl;
    
    int total = 0;
    int passed = 0;
    
    #define RUN_TEST(test_func) \
        total++; \
        if (test_func()) { passed++; } \
        std::cout << std::endl;
    
    RUN_TEST(test_set_exchange);
    RUN_TEST(test_start_stop);
    RUN_TEST(test_watch_list);
    RUN_TEST(test_get_status);
    RUN_TEST(test_immediate_scan);
    
    std::cout << "========== Test Results ==========" << std::endl;
    std::cout << "Total: " << total << ", Passed: " << passed << ", Failed: " << (total - passed) << std::endl;
    
    return (passed == total) ? 0 : 1;
}
