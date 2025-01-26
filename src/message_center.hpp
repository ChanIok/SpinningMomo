#pragma once
#include <unordered_map>
#include <functional>
#include <variant>
#include "parameter_types.hpp"

// 消息类型枚举
enum class MessageType {
    ParameterUpdated,
    WindowResized,
    StatusChanged,
    ErrorOccurred
};

// 参数更新数据
struct ParameterUpdateData {
    ParameterType type;
    float value;
    float confidence;
};

// 消息数据结构
struct MessageData {
    MessageType type;
    std::variant<
        std::monostate,
        ParameterUpdateData
    > data;
};

// 消息中心类
class MessageCenter {
public:
    using MessageCallback = std::function<void(const MessageData&)>;

    static MessageCenter& Instance() {
        static MessageCenter instance;
        return instance;
    }

    // 注册消息处理函数
    void Subscribe(MessageType type, void* owner, MessageCallback callback) {
        m_handlers[type][owner] = std::move(callback);
    }

    // 取消注册
    void Unsubscribe(MessageType type, void* owner) {
        if (auto it = m_handlers.find(type); it != m_handlers.end()) {
            it->second.erase(owner);
        }
    }

    // 发送消息
    void SendMessage(const MessageData& msg) {
        if (auto it = m_handlers.find(msg.type); it != m_handlers.end()) {
            for (const auto& [owner, callback] : it->second) {
                callback(msg);
            }
        }
    }

private:
    std::unordered_map<
        MessageType,
        std::unordered_map<void*, MessageCallback>
    > m_handlers;
}; 