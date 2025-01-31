#include "config_manager.hpp"
#include <tchar.h>
#include <shellapi.h>
#include <shlwapi.h>

ConfigManager::ConfigManager() {
}

void ConfigManager::Initialize() {
    // 获取程序所在目录
    TCHAR exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    
    // 获取程序所在目录
    m_configPath = exePath;
    size_t lastSlash = m_configPath.find_last_of(TEXT("\\"));
    if (lastSlash != std::wstring::npos) {
        m_configPath = m_configPath.substr(0, lastSlash + 1);
    }
    m_configPath += Constants::CONFIG_FILE;

    // 检查配置文件是否存在
    if (GetFileAttributes(m_configPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        // 创建默认配置文件
        WritePrivateProfileString(Constants::WINDOW_SECTION, Constants::WINDOW_TITLE, TEXT(""), m_configPath.c_str());
        WritePrivateProfileString(Constants::HOTKEY_SECTION, Constants::HOTKEY_MODIFIERS, TEXT("3"), m_configPath.c_str());
        WritePrivateProfileString(Constants::HOTKEY_SECTION, Constants::HOTKEY_KEY, TEXT("82"), m_configPath.c_str());
        WritePrivateProfileString(Constants::CUSTOM_RATIO_SECTION, Constants::CUSTOM_RATIO_LIST, TEXT(""), m_configPath.c_str());
        WritePrivateProfileString(Constants::CUSTOM_RESOLUTION_SECTION, Constants::CUSTOM_RESOLUTION_LIST, TEXT(""), m_configPath.c_str());
        WritePrivateProfileString(Constants::MENU_SECTION, Constants::MENU_FLOATING, TEXT("1"), m_configPath.c_str());
        WritePrivateProfileString(Constants::SCREENSHOT_SECTION, Constants::SCREENSHOT_PATH, TEXT(""), m_configPath.c_str());
    }
}

void ConfigManager::LoadAllConfigs() {
    LoadHotkeyConfig();
    LoadWindowConfig();
    LoadLanguageConfig();
    LoadTaskbarConfig();
    LoadMenuConfig();
    LoadGameAlbumConfig();
}

void ConfigManager::SaveAllConfigs() {
    SaveHotkeyConfig();
    SaveWindowConfig();
    SaveLanguageConfig();
    SaveTaskbarConfig();
    SaveMenuConfig();
    SaveGameAlbumConfig();
}

void ConfigManager::LoadHotkeyConfig() {
    TCHAR buffer[32];
    // 读取修饰键
    if (GetPrivateProfileString(Constants::HOTKEY_SECTION, 
                              Constants::HOTKEY_MODIFIERS,
                              TEXT(""), buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        m_hotkeyModifiers = _wtoi(buffer);
    }

    // 读取主键
    if (GetPrivateProfileString(Constants::HOTKEY_SECTION,
                              Constants::HOTKEY_KEY,
                              TEXT(""), buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        m_hotkeyKey = _wtoi(buffer);
    }
}

void ConfigManager::SaveHotkeyConfig() {
    TCHAR buffer[32];
    // 保存修饰键
    _stprintf_s(buffer, _countof(buffer), TEXT("%u"), m_hotkeyModifiers);
    WritePrivateProfileString(Constants::HOTKEY_SECTION,
                            Constants::HOTKEY_MODIFIERS,
                            buffer, m_configPath.c_str());

    // 保存主键
    _stprintf_s(buffer, _countof(buffer), TEXT("%u"), m_hotkeyKey);
    WritePrivateProfileString(Constants::HOTKEY_SECTION,
                            Constants::HOTKEY_KEY,
                            buffer, m_configPath.c_str());
}

void ConfigManager::LoadWindowConfig() {
    TCHAR buffer[256];
    if (GetPrivateProfileString(Constants::WINDOW_SECTION,
                              Constants::WINDOW_TITLE,
                              TEXT(""), buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        m_windowTitle = buffer;
    }
}

void ConfigManager::SaveWindowConfig() {
    if (!m_windowTitle.empty()) {
        WritePrivateProfileString(Constants::WINDOW_SECTION,
                                Constants::WINDOW_TITLE,
                                m_windowTitle.c_str(),
                                m_configPath.c_str());
    }
}

void ConfigManager::LoadLanguageConfig() {
    TCHAR buffer[32];
    if (GetPrivateProfileString(Constants::LANG_SECTION,
                              Constants::LANG_CURRENT,
                              TEXT(""), buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        m_language = buffer;
    } else {
        LANGID langId = GetUserDefaultUILanguage();
        WORD primaryLangId = PRIMARYLANGID(langId);
        m_language = (primaryLangId == LANG_CHINESE) ? 
            Constants::LANG_ZH_CN : Constants::LANG_EN_US;
        SaveLanguageConfig();
    }
}

void ConfigManager::SaveLanguageConfig() {
    WritePrivateProfileString(Constants::LANG_SECTION,
                            Constants::LANG_CURRENT,
                            m_language.c_str(),
                            m_configPath.c_str());
}

void ConfigManager::LoadTaskbarConfig() {
    TCHAR buffer[32];
    if (GetPrivateProfileString(Constants::TASKBAR_SECTION,
                              Constants::TASKBAR_AUTOHIDE,
                              TEXT("0"), buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        m_taskbarAutoHide = (_wtoi(buffer) != 0);
    }
    
    if (GetPrivateProfileString(Constants::TASKBAR_SECTION,
                              Constants::TASKBAR_LOWER,
                              TEXT("1"), buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        m_taskbarLower = (_wtoi(buffer) != 0);
    }
}

void ConfigManager::SaveTaskbarConfig() {
    WritePrivateProfileString(Constants::TASKBAR_SECTION,
                            Constants::TASKBAR_AUTOHIDE,
                            m_taskbarAutoHide ? TEXT("1") : TEXT("0"),
                            m_configPath.c_str());
                            
    WritePrivateProfileString(Constants::TASKBAR_SECTION,
                            Constants::TASKBAR_LOWER,
                            m_taskbarLower ? TEXT("1") : TEXT("0"),
                            m_configPath.c_str());
}

void ConfigManager::LoadMenuConfig() {
    TCHAR buffer[32];
    if (GetPrivateProfileString(Constants::MENU_SECTION,
                              Constants::MENU_FLOATING,
                              TEXT("0"), buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        m_useFloatingWindow = (_wtoi(buffer) != 0);
    }
}

void ConfigManager::SaveMenuConfig() {
    WritePrivateProfileString(Constants::MENU_SECTION,
                            Constants::MENU_FLOATING,
                            m_useFloatingWindow ? TEXT("1") : TEXT("0"),
                            m_configPath.c_str());
}

void ConfigManager::LoadGameAlbumConfig() {
    TCHAR buffer[MAX_PATH];
    if (GetPrivateProfileString(Constants::SCREENSHOT_SECTION,
                              Constants::SCREENSHOT_PATH,
                              TEXT(""), buffer, MAX_PATH,
                              m_configPath.c_str()) > 0) {
        m_gameAlbumPath = buffer;
    }
}

void ConfigManager::SaveGameAlbumConfig() {
    if (!m_gameAlbumPath.empty()) {
        WritePrivateProfileString(Constants::SCREENSHOT_SECTION,
                                Constants::SCREENSHOT_PATH,
                                m_gameAlbumPath.c_str(),
                                m_configPath.c_str());
    }
}

bool ConfigManager::AddCustomRatio(const std::wstring& ratio, std::vector<AspectRatio>& ratios) {
    size_t colonPos = ratio.find(TEXT(":"));
    if (colonPos == std::wstring::npos) return false;
    
    try {
        double width = std::stod(ratio.substr(0, colonPos));
        double height = std::stod(ratio.substr(colonPos + 1));
        if (height <= 0) return false;
        
        ratios.emplace_back(ratio, width/height);
        return true;
    } catch (...) {
        return false;
    }
}

ConfigLoadResult ConfigManager::LoadCustomRatios(std::vector<AspectRatio>& ratios, const LocalizedStrings& strings) {
    TCHAR buffer[1024];
    if (GetPrivateProfileString(Constants::CUSTOM_RATIO_SECTION,
                              Constants::CUSTOM_RATIO_LIST,
                              TEXT(""), buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        std::wstring ratiosStr = buffer;
        if (ratiosStr.empty()) return ConfigLoadResult();

        bool hasError = false;
        std::wstring errorDetails;
        
        size_t start = 0, end = 0;
        while ((end = ratiosStr.find(TEXT(","), start)) != std::wstring::npos) {
            std::wstring ratio = ratiosStr.substr(start, end - start);
            if (!AddCustomRatio(ratio, ratios)) {
                hasError = true;
                errorDetails += ratio + TEXT(", ");
            }
            start = end + 1;
        }
        
        if (start < ratiosStr.length()) {
            std::wstring ratio = ratiosStr.substr(start);
            if (!AddCustomRatio(ratio, ratios)) {
                hasError = true;
                errorDetails += ratio;
            }
        }

        if (hasError) {
            return ConfigLoadResult(strings.CONFIG_FORMAT_ERROR + errorDetails + TEXT("\n") + strings.RATIO_FORMAT_EXAMPLE);
        }
    }
    return ConfigLoadResult();
}

bool ConfigManager::AddCustomResolution(const std::wstring& resolution, std::vector<ResolutionPreset>& resolutions) {
    try {
        size_t xPos = resolution.find(TEXT("x"));
        if (xPos == std::wstring::npos) return false;

        int width = std::stoi(resolution.substr(0, xPos));
        int height = std::stoi(resolution.substr(xPos + 1));

        if (width <= 0 || height <= 0) return false;

        std::wstring name = resolution;
        resolutions.emplace_back(name, width, height);
        return true;
    } catch (...) {
        return false;
    }
}

ConfigLoadResult ConfigManager::LoadCustomResolutions(std::vector<ResolutionPreset>& resolutions, const LocalizedStrings& strings) {
    TCHAR buffer[1024];
    if (GetPrivateProfileString(Constants::CUSTOM_RESOLUTION_SECTION,
                              Constants::CUSTOM_RESOLUTION_LIST,
                              TEXT(""), buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        std::wstring resolutionsStr = buffer;
        if (resolutionsStr.empty()) return ConfigLoadResult();

        bool hasError = false;
        std::wstring errorDetails;

        size_t start = 0, end = 0;
        while ((end = resolutionsStr.find(TEXT(","), start)) != std::wstring::npos) {
            std::wstring resolution = resolutionsStr.substr(start, end - start);
            if (!AddCustomResolution(resolution, resolutions)) {
                hasError = true;
                errorDetails += resolution + TEXT(", ");
            }
            start = end + 1;
        }
        
        if (start < resolutionsStr.length()) {
            std::wstring resolution = resolutionsStr.substr(start);
            if (!AddCustomResolution(resolution, resolutions)) {
                hasError = true;
                errorDetails += resolution;
            }
        }

        if (hasError) {
            return ConfigLoadResult(strings.CONFIG_FORMAT_ERROR + errorDetails + TEXT("\n") + strings.RESOLUTION_FORMAT_EXAMPLE);
        }
    }
    return ConfigLoadResult();
} 