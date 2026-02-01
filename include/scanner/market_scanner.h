#pragma once

#include "managers/strategy_manager.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

 

class MarketScanner {
public:
    MarketScanner();
    ~MarketScanner();
    
    // 启动/停止扫描
    void start();
    void stop();
    bool isRunning() const { return running_; }
    
    // 立即执行一次扫描
    void scanOnce();
    
private:
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> scan_thread_;
    
    void scanLoop();
    std::vector<ScanResult> performScan();
    
    // 筛选条件
    bool meetsSelectionCriteria(const ScanResult& result);
    double calculateScore(const ScanResult& result);
};

 
