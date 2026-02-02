#include "utils/logger.h"
#include "utils/stringsUtils.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    // 创建 logs 目录（如果不存在）
    try {
        std::filesystem::create_directories("logs");
    } catch (const std::exception& e) {
        std::cerr << "Failed to create logs directory: " << e.what() << std::endl;
    }
    
    // 打开日志文件，文件名带时间戳以便区分每次运行
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &now_c);
#else
    localtime_r(&now_c, &tm);
#endif
    std::stringstream ts_ss;
    ts_ss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    std::string timestamp = ts_ss.str();

    std::string log_path = "logs/trading_system_" + timestamp + ".log";
    log_file_.open(log_path, std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open log file: " << log_path << std::endl;
    }
}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < min_level_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string log_entry = getCurrentTime() + " [" + levelToString(level) + "] " + message;
    
    // 输出到控制台
    PrintToConsole(log_entry);
    
    // 输出到文件
    if (log_file_.is_open()) {
        log_file_ << log_entry << std::endl;
        log_file_.flush();
    }
}

std::string Logger::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

