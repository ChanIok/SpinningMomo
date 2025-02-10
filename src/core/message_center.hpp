#pragma once
#include <unordered_map>
#include <functional>
#include <variant>
#include <any>
#include <string>

// 消息类型枚举
enum class MessageType {
    WindowResized,
    StatusChanged,
    ErrorOccurred
};

// 通用消息数据结构
struct MessageData {
    MessageType type;
    std::any data;           // 使用std::any来存储任意类型的数据
    std::string description; // 可选的消息描述
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

    // 取消某个所有者的所有订阅
    void UnsubscribeAll(void* owner) {
        for (auto& [type, handlers] : m_handlers) {
            handlers.erase(owner);
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
    MessageCenter() = default;
    ~MessageCenter() = default;
    MessageCenter(const MessageCenter&) = delete;
    MessageCenter& operator=(const MessageCenter&) = delete;

    std::unordered_map<
        MessageType,
        std::unordered_map<void*, MessageCallback>
    > m_handlers;
}; 