#include "utils/logger.h"
#include "utils/stringsUtils.h"
#include "common/object.h"
#include "event/event.h"
#include "event/event_type.h"
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>

#ifdef _WIN32
    #include <direct.h>
    #define CREATE_DIR(path) _mkdir(path)
#else
    #include <sys/stat.h>
    #define CREATE_DIR(path) mkdir(path, 0755)
#endif

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    // 创建 logs 目录（如果不存在）
    // 跨平台支持：Windows (_mkdir) 和 Unix/macOS (mkdir)
    try {
        CREATE_DIR("logs");
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

void Logger::handld_logs(const EventPtr& event) {
    if (!event) {
        std::cerr << "[Logger] ERROR: Event is null" << std::endl;
        return;
    }
    
    // 优先尝试从std::any获取LogData（用于主程序内的日志）
    const LogData* data = event->getData<LogData>();
    if (data) {
        log(data->level, data->message);
        return;
    }
    
    // 回退方案：从Event的extras获取日志数据
    // 这用于来自dylib的日志，因为dylib中的LogData类型信息与主程序不同
    std::string level_str = event->getExtra("level");
    std::string message = event->getExtra("message");
    
    if (!message.empty()) {
        // 将level字符串转换回LogLevel
        LogLevel level = LogLevel::Info; // 默认值
        if (level_str == "Debug") {
            level = LogLevel::Debug;
        } else if (level_str == "Info") {
            level = LogLevel::Info;
        } else if (level_str == "Warn") {
            level = LogLevel::Warn;
        } else if (level_str == "Error") {
            level = LogLevel::Error;
        }
        
        log(level, message);
    } else {
        // 两种方式都失败了，输出调试信息
        std::cerr << "[Logger] WARNING: Unable to extract log data from event" << std::endl;
        std::cerr << "  Event type: " << eventTypeToString(event->getType()) << std::endl;
        
        if (!event->hasData()) {
            std::cerr << "  Event has no std::any data set" << std::endl;
        } else {
            std::cerr << "  std::any cast failed (possible cross-dylib issue)" << std::endl;
            std::cerr << "  std::any type: " << event->getAnyType().name() << std::endl;
        }
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

