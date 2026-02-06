#include "notification/telegram_sender.h"
#include "utils/logger.h"
#include <httplib.h>
#include <sstream>
#include <iomanip>

TelegramSender::TelegramSender(const std::string& bot_token, const std::string& chat_id, int timeout_seconds)
    : bot_token_(bot_token), chat_id_(chat_id), timeout_seconds_(timeout_seconds) {
    if (bot_token_.empty() || chat_id_.empty()) {
        LOG_WARN("TelegramSender initialized with empty token or chat_id");
    }
}

TelegramSender::~TelegramSender() = default;

bool TelegramSender::send(const NotificationMessage& message) {
    if (!isReady()) {
        LOG_ERROR("TelegramSender is not ready (missing token or chat_id)");
        return false;
    }
    
    std::string formatted_message = formatMessage(message);
    LOG_DEBUG(std::string("Sending Telegram message: ") + formatted_message);
    
    return sendHttpRequest(formatted_message);
}

bool TelegramSender::isReady() const {
    return !bot_token_.empty() && !chat_id_.empty();
}

bool TelegramSender::testConnection() {
    if (!isReady()) {
        LOG_ERROR("TelegramSender is not ready for testing");
        return false;
    }
    
    try {
        httplib::Client cli("https://api.telegram.org");
        cli.set_connection_timeout(timeout_seconds_, 0);
        cli.set_read_timeout(timeout_seconds_, 0);
        
        std::string endpoint = "/bot" + bot_token_ + "/getMe";
        auto res = cli.Get(endpoint);
        
        if (res && res->status == 200) {
            LOG_INFO("Telegram Bot connection test successful");
            return true;
        } else {
            LOG_ERROR(std::string("Telegram Bot connection test failed, status: ") + std::to_string(res ? res->status : 0));
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception during Telegram connection test: ") + e.what());
        return false;
    }
}

std::shared_ptr<TelegramSender> TelegramSender::createFromConfig(const nlohmann::json& config) {
    try {
        if (!config.contains("bot_token") || !config.contains("chat_id")) {
            LOG_ERROR("Config missing required fields: bot_token or chat_id");
            return nullptr;
        }
        
        std::string bot_token = config["bot_token"];
        std::string chat_id = config["chat_id"];
        
        if (bot_token.empty() || chat_id.empty()) {
            LOG_ERROR("bot_token or chat_id is empty");
            return nullptr;
        }
        
        int timeout_seconds = config.value("api_timeout_seconds", 5);
        
        auto sender = std::make_shared<TelegramSender>(bot_token, chat_id, timeout_seconds);
        
        if (!sender->isReady()) {
            LOG_ERROR("TelegramSender created but not ready");
            return nullptr;
        }
        
        LOG_INFO(std::string("TelegramSender created successfully with timeout: ") + std::to_string(timeout_seconds) + "s");
        return sender;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception creating TelegramSender from config: ") + e.what());
        return nullptr;
    }
}

std::string TelegramSender::formatMessage(const NotificationMessage& message) const {
    std::stringstream ss;
    ss << "[" << message.type << "] ";
    
    // 添加时间戳
    auto time_t_val = std::chrono::milliseconds(message.timestamp);
    auto duration = time_t_val.count();
    auto sctp = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(time_t_val)
    );
    auto time_point = std::chrono::system_clock::to_time_t(sctp);
    
    ss << std::put_time(std::localtime(&time_point), "%Y-%m-%d %H:%M:%S") << "\n";
    ss << message.content;
    
    if (!message.id.empty()) {
        ss << "\n(ID: " << message.id << ")";
    }
    
    return ss.str();
}

bool TelegramSender::sendHttpRequest(const std::string& message_text) {
    try {
        httplib::Client cli("https://api.telegram.org");
        cli.set_connection_timeout(timeout_seconds_, 0);
        cli.set_read_timeout(timeout_seconds_, 0);
        
        // 构建请求URL
        std::string endpoint = "/bot" + bot_token_ + "/sendMessage";
        
        // 构建POST参数
        httplib::Params params;
        params.emplace("chat_id", chat_id_);
        params.emplace("text", message_text);
        params.emplace("parse_mode", "HTML");
        
        auto res = cli.Post(endpoint, params);
        
        if (res && res->status == 200) {
            LOG_INFO("Message sent to Telegram successfully");
            return true;
        } else {
            LOG_ERROR(std::string("Failed to send message to Telegram, status: ") + std::to_string(res ? res->status : 0));
            if (res) {
                LOG_ERROR(std::string("Response body: ") + res->body);
            }
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception sending HTTP request to Telegram: ") + e.what());
        return false;
    }
}
