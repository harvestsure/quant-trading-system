#pragma once

#include "event_type.h"
#include "common/object.h"
#include <memory>
#include <any>
#include <map>
#include <string>

 

// 事件类
class Event {
public:
    Event(EventType type) : type_(type), timestamp_(getCurrentTimestamp()) {}
    
    EventType getType() const { return type_; }
    int64_t getTimestamp() const { return timestamp_; }
    
    // 模板方法设置数据
    template<typename T>
    void setData(const T& data) {
        data_ = data;
    }
    
    // 模板方法获取数据
    template<typename T>
    const T* getData() const {
        try {
            return std::any_cast<T>(&data_);
        } catch (...) {
            return nullptr;
        }
    }
    
    // 设置/获取附加信息
    void setExtra(const std::string& key, const std::string& value) {
        extras_[key] = value;
    }
    
    std::string getExtra(const std::string& key) const {
        auto it = extras_.find(key);
        return it != extras_.end() ? it->second : "";
    }
    
private:
    EventType type_;
    int64_t timestamp_;
    std::any data_;
    std::map<std::string, std::string> extras_;
    
    static int64_t getCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
};

using EventPtr = std::shared_ptr<Event>;

 
