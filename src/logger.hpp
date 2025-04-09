#pragma once
#include <windows.h>
#include <string>
#include <mutex>
#include <fstream>
#include <sstream>

// 日志级别
enum class LogLevel {
    DEBUG,
    INFO,
    ERR
};

// 使用宏简化日志调用
#define LOG_DEBUG(message, ...) Logger::GetInstance().Log(LogLevel::DEBUG, __FUNCTION__, message, ##__VA_ARGS__)
#define LOG_INFO(message, ...) Logger::GetInstance().Log(LogLevel::INFO, __FUNCTION__, message, ##__VA_ARGS__)
#define LOG_ERROR(message, ...) Logger::GetInstance().Log(LogLevel::ERR, __FUNCTION__, message, ##__VA_ARGS__)

// 日志记录类 (单例模式)
class Logger {
public:
    // 获取单例实例
    static Logger& GetInstance();
    
    // 初始化日志系统
    bool Initialize();
    
    // 记录日志
    template<typename... Args>
    void Log(LogLevel level, const char* function, const std::string& format, Args&&... args);
    
    // 获取当前日志级别的字符串表示
    static const char* GetLevelString(LogLevel level);

    // 设置日志级别
    void SetLogLevel(LogLevel level);
    
    // 获取当前日志级别
    LogLevel GetLogLevel() const;

private:
    Logger();
    ~Logger();
    
    // 禁止复制和移动
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;
    
    // 格式化消息
    template<typename... Args>
    std::string FormatMessage(const std::string& format, Args&&... args);
    
    // 获取当前时间的字符串表示
    std::string GetTimestamp();
    
    std::mutex m_mutex;           // 用于线程安全
    std::ofstream m_logFile;      // 日志文件
    bool m_initialized = false;   // 初始化标志
    LogLevel m_currentLogLevel = LogLevel::INFO;  // 当前日志级别，默认为INFO
};

// 模板函数实现
template<typename... Args>
void Logger::Log(LogLevel level, const char* function, const std::string& format, Args&&... args) {
    if (!m_initialized && !Initialize()) {
        return;
    }
    
    // 如果日志级别低于当前设置的级别，则忽略
    if (level < m_currentLogLevel) {
        return;
    }
    
    std::string message = FormatMessage(format, std::forward<Args>(args)...);
    std::string timestamp = GetTimestamp();
    std::string levelStr = GetLevelString(level);
    
    std::ostringstream logStream;
    logStream << "[" << timestamp << "] [" << levelStr << "] [" << function << "]: " << message;
    std::string logMessage = logStream.str();
    
    // 线程安全地写入日志
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 写入文件
        if (m_logFile.is_open()) {
            m_logFile << logMessage << std::endl;
            m_logFile.flush();
        }
        
        // 输出到调试窗口
        OutputDebugStringA((logMessage + "\n").c_str());
    }
}

template<typename... Args>
std::string Logger::FormatMessage(const std::string& format, Args&&... args) {
    if constexpr (sizeof...(args) == 0) {
        return format;
    } else {
        // 简单计算长度，避免多次分配内存
        int size = snprintf(nullptr, 0, format.c_str(), std::forward<Args>(args)...);
        if (size <= 0) {
            return format;
        }
        
        // 分配内存并格式化
        std::string result;
        result.resize(size + 1);
        snprintf(&result[0], size + 1, format.c_str(), std::forward<Args>(args)...);
        result.resize(size);  // 移除末尾的null字符
        
        return result;
    }
}