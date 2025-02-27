#include "logger.hpp"
#include <ctime>
#include <chrono>
#include <iomanip>
#include <ShlObj.h>
#include <Shlwapi.h>

// 获取单例实例
Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
#ifdef NDEBUG
    // Release 版本下，默认设置为 INFO 级别
    m_currentLogLevel = LogLevel::INFO;
#else
    // Debug 版本下，默认设置为 DEBUG 级别
    m_currentLogLevel = LogLevel::DEBUG;
#endif
}

// 析构函数 - 关闭日志文件
Logger::~Logger() {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
}

// 初始化日志系统
bool Logger::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        return true;
    }
    
    // 获取程序所在的目录
    wchar_t exePath[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) {
        return false;
    }
    
    // 提取目录部分
    PathRemoveFileSpecW(exePath);
    
    // 日志文件放在程序所在目录下
    std::wstring logFilePath = std::wstring(exePath) + L"\\app.log";
    m_logFile.open(logFilePath, std::ios::out | std::ios::trunc);
    if (!m_logFile.is_open()) {
        return false;
    }
    
    m_initialized = true;
    
    // 记录初始日志
    m_logFile << "[" << GetTimestamp() << "] ---------- Application Started ----------" << std::endl;
    OutputDebugStringA(("[" + GetTimestamp() + "] ---------- Application Started ----------\n").c_str());
    
    return true;
}

// 设置日志级别
void Logger::SetLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentLogLevel = level;
}

// 获取当前日志级别
LogLevel Logger::GetLogLevel() const {
    return m_currentLogLevel;
}

// 获取日志级别的字符串表示
const char* Logger::GetLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::ERR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

// 获取当前时间的字符串表示
std::string Logger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::ostringstream timestamp;
    std::tm tm_buf;
    localtime_s(&tm_buf, &time_t_now);
    
    timestamp << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << '.' 
              << std::setfill('0') << std::setw(3) << ms.count();
              
    return timestamp.str();
}