#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <Windows.h>

class ConfigManager {
public:
    struct Config {
        // 热键配置
        struct {
            bool ctrl{true};
            bool shift{true};
            UINT key{'P'};  // 默认为P键
        } hotkey;

        // 窗口配置
        struct {
            bool mirrorX{false};
            bool mirrorY{false};
            float scaleRatio{0.4f};
        } window;
    };

    static ConfigManager& Instance();
    
    bool LoadConfig();
    bool SaveConfig();
    
    Config& GetConfig() { return config; }
    const Config& GetConfig() const { return config; }

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    std::string GetConfigPath() const;
    
    Config config;
    static const char* CONFIG_FILENAME;
};