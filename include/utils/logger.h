#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include "logger_defines.h"
#include "event/event_interface.h"

class Logger {
public:
    static Logger& getInstance();
    
    void log(LogLevel level, const std::string& message);
    void setLogLevel(LogLevel level) { min_level_ = level; }

    void handld_logs(const EventPtr&);
    
    // 禁止拷贝
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
private:
    Logger();
    ~Logger();
    
    std::ofstream log_file_;
    LogLevel min_level_ = LogLevel::Info;
    std::mutex mutex_;
    
    std::string getCurrentTime();
};

// 便捷宏
#define LOG_DEBUG(msg) Logger::getInstance().log(LogLevel::Debug, msg)
#define LOG_INFO(msg) Logger::getInstance().log(LogLevel::Info, msg)
#define LOG_WARN(msg) Logger::getInstance().log(LogLevel::Warn, msg)
#define LOG_ERROR(msg) Logger::getInstance().log(LogLevel::Error, msg)
