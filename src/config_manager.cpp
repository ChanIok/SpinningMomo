#include "config_manager.h"
#include <fstream>
#include <filesystem>

const char* ConfigManager::CONFIG_FILENAME = "config.json";

ConfigManager& ConfigManager::Instance()
{
    static ConfigManager instance;
    return instance;
}

std::string ConfigManager::GetConfigPath() const
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::filesystem::path configPath = std::filesystem::path(exePath).parent_path() / CONFIG_FILENAME;
    return configPath.string();
}

bool ConfigManager::LoadConfig()
{
    std::string configPath = GetConfigPath();
    
    // 检查配置文件是否存在
    if (!std::filesystem::exists(configPath)) {
        // 文件不存在，使用默认配置并保存
        config = Config{};  // 使用默认构造函数中的默认值
        return SaveConfig();
    }

    try {
        std::ifstream file(GetConfigPath());
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json j;
        file >> j;

        // 读取热键配置
        if (j.contains("hotkey")) {
            config.hotkey.ctrl = j["hotkey"]["ctrl"].get<bool>();
            config.hotkey.shift = j["hotkey"]["shift"].get<bool>();
            config.hotkey.alt = j["hotkey"]["alt"].get<bool>();
            config.hotkey.key = j["hotkey"]["key"].get<UINT>();
        }

        // 读取窗口配置
        if (j.contains("window")) {
            config.window.mirrorX = j["window"]["mirrorX"].get<bool>();
            config.window.mirrorY = j["window"]["mirrorY"].get<bool>();
            config.window.scaleRatio = j["window"]["scaleRatio"].get<float>();
        }

        return true;
    }
    catch (const std::exception&) {
        // 如果读取失败，使用默认配置并尝试保存
        config = Config{};
        return SaveConfig();
    }
}

bool ConfigManager::SaveConfig()
{
    try {
        nlohmann::json j;

        // 保存热键配置
        j["hotkey"]["ctrl"] = config.hotkey.ctrl;
        j["hotkey"]["shift"] = config.hotkey.shift;
        j["hotkey"]["alt"] = config.hotkey.alt;
        j["hotkey"]["key"] = config.hotkey.key;

        // 保存窗口配置
        j["window"]["mirrorX"] = config.window.mirrorX;
        j["window"]["mirrorY"] = config.window.mirrorY;
        j["window"]["scaleRatio"] = config.window.scaleRatio;

        std::ofstream file(GetConfigPath());
        if (!file.is_open()) {
            return false;
        }

        file << j.dump(4);
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}