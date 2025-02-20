#pragma once
#include <unordered_map>
#include <mutex>
#include <string>
#include <windows.h>
#include <TlHelp32.h>

class ProcessManager {
public:
    static ProcessManager& getInstance() {
        static ProcessManager instance;
        return instance;
    }

    bool isTargetProcess(DWORD pid) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 先查找缓存
        auto it = m_processCache.find(pid);
        if (it != m_processCache.end()) {
            return it->second;
        }

        // 缓存未命中，获取进程名并缓存结果
        std::string processName = GetProcessNameByPID(pid);
        bool isTarget = (processName == "X6Game-Win64-Shipping.exe" || 
                        processName == "SpinningMomo.exe");
        m_processCache[pid] = isTarget;
        
        return isTarget;
    }

    void clearCache() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_processCache.clear();
    }

private:
    ProcessManager() = default;
    ~ProcessManager() = default;
    ProcessManager(const ProcessManager&) = delete;
    ProcessManager& operator=(const ProcessManager&) = delete;

    std::string GetProcessNameByPID(DWORD pid) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return "";
        }

        PROCESSENTRY32W processEntry = { 0 };
        processEntry.dwSize = sizeof(processEntry);

        if (Process32FirstW(snapshot, &processEntry)) {
            do {
                if (processEntry.th32ProcessID == pid) {
                    CloseHandle(snapshot);
                    char process_name[MAX_PATH];
                    wcstombs(process_name, processEntry.szExeFile, MAX_PATH);
                    return std::string(process_name);
                }
            } while (Process32NextW(snapshot, &processEntry));
        }

        CloseHandle(snapshot);
        return "";
    }

private:
    std::unordered_map<DWORD, bool> m_processCache;
    std::mutex m_mutex;
}; 