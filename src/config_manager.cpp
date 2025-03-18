#include "config_manager.hpp"
#include <tchar.h>
#include <shellapi.h>
#include <shlwapi.h>

ConfigManager::ConfigManager() {
}

void ConfigManager::Initialize() {
    // 获取程序所在目录
    wchar_t exePath[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) {
        // 获取失败，使用当前目录
        m_configPath = L".\\";
    } else {
        // 提取目录部分
        PathRemoveFileSpecW(exePath);
        m_configPath = std::wstring(exePath) + L"\\";
    }
    
    m_configPath += Constants::CONFIG_FILE;

    // 检查配置文件是否存在
    if (GetFileAttributes(m_configPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        // 创建默认配置文件
        WritePrivateProfileString(Constants::LOGGER_SECTION, Constants::LOGGER_LEVEL, TEXT("INFO"), m_configPath.c_str());
        WritePrivateProfileString(Constants::WINDOW_SECTION, Constants::WINDOW_TITLE, TEXT(""), m_configPath.c_str());
        WritePrivateProfileString(Constants::HOTKEY_SECTION, Constants::HOTKEY_MODIFIERS, TEXT("3"), m_configPath.c_str());
        WritePrivateProfileString(Constants::HOTKEY_SECTION, Constants::HOTKEY_KEY, TEXT("82"), m_configPath.c_str());
        WritePrivateProfileString(Constants::MENU_SECTION, Constants::MENU_FLOATING, TEXT("1"), m_configPath.c_str());
        WritePrivateProfileString(Constants::SCREENSHOT_SECTION, Constants::SCREENSHOT_PATH, TEXT(""), m_configPath.c_str());
        WritePrivateProfileString(Constants::MENU_SECTION, Constants::MENU_ITEMS, 
                                TEXT("CaptureWindow,OpenScreenshot,PreviewWindow,OverlayWindow,Reset,Close"), 
                                m_configPath.c_str());
        
        // 添加默认的宽高比和分辨率配置
        WritePrivateProfileString(Constants::MENU_SECTION, Constants::ASPECT_RATIO_ITEMS, 
                                TEXT("32:9,21:9,16:9,3:2,1:1,2:3,9:16"), 
                                m_configPath.c_str());
        WritePrivateProfileString(Constants::MENU_SECTION, Constants::RESOLUTION_ITEMS, 
                                TEXT("Default,4K,6K,8K,12K"), 
                                m_configPath.c_str());
        
        // 添加默认的黑边模式配置（默认为禁用）
        WritePrivateProfileString(Constants::LETTERBOX_SECTION, Constants::LETTERBOX_ENABLED, 
                                TEXT("0"), 
                                m_configPath.c_str());
    }
}

void ConfigManager::LoadAllConfigs() {
    LoadHotkeyConfig();
    LoadWindowConfig();
    LoadLanguageConfig();
    LoadTaskbarConfig();
    LoadMenuConfig();
    LoadGameAlbumConfig();
    LoadLogConfig();
    LoadLetterboxConfig();
}

void ConfigManager::LoadLogConfig() {
    TCHAR buffer[32];
    if (GetPrivateProfileString(Constants::LOGGER_SECTION, 
                               Constants::LOGGER_LEVEL,
                               TEXT(""), 
                               buffer, 
                               _countof(buffer),
                               m_configPath.c_str()) > 0) {
        std::wstring logLevelStr = buffer;
        
        if (logLevelStr == TEXT("DEBUG")) {
            m_logLevel = LogLevel::DEBUG;
        } else if (logLevelStr == TEXT("INFO")) {
            m_logLevel = LogLevel::INFO;
        } else if (logLevelStr == TEXT("ERROR")) {
            m_logLevel = LogLevel::ERR;
        }
    }
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
    TCHAR buffer[1024];
    
    // 读取浮动窗口设置
    if (GetPrivateProfileString(Constants::MENU_SECTION,
                              Constants::MENU_FLOATING,
                              TEXT("1"), buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        m_useFloatingWindow = (_wtoi(buffer) != 0);
    }
    
    // 读取菜单项显示配置
    if (GetPrivateProfileString(Constants::MENU_SECTION,
                              Constants::MENU_ITEMS,
                              TEXT(""), buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        std::wstring itemsStr = buffer;
        if (!itemsStr.empty()) {
            m_menuItemsToShow.clear();
            
            size_t start = 0, end = 0;
            while ((end = itemsStr.find(TEXT(","), start)) != std::wstring::npos) {
                std::wstring item = itemsStr.substr(start, end - start);
                if (!item.empty()) {
                    m_menuItemsToShow.push_back(item);
                }
                start = end + 1;
            }
            
            if (start < itemsStr.length()) {
                std::wstring item = itemsStr.substr(start);
                if (!item.empty()) {
                    m_menuItemsToShow.push_back(item);
                }
            }
        }
    }
    
    // 读取宽高比项配置
    if (GetPrivateProfileString(Constants::MENU_SECTION,
                              Constants::ASPECT_RATIO_ITEMS,
                              TEXT(""), buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        std::wstring ratioItemsStr = buffer;
        if (!ratioItemsStr.empty()) {
            m_aspectRatioItems.clear();
            
            size_t start = 0, end = 0;
            while ((end = ratioItemsStr.find(TEXT(","), start)) != std::wstring::npos) {
                std::wstring item = ratioItemsStr.substr(start, end - start);
                if (!item.empty()) {
                    m_aspectRatioItems.push_back(item);
                }
                start = end + 1;
            }
            
            if (start < ratioItemsStr.length()) {
                std::wstring item = ratioItemsStr.substr(start);
                if (!item.empty()) {
                    m_aspectRatioItems.push_back(item);
                }
            }
        }
    }
    
    // 读取分辨率项配置
    if (GetPrivateProfileString(Constants::MENU_SECTION,
                              Constants::RESOLUTION_ITEMS,
                              TEXT(""), buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        std::wstring resolutionItemsStr = buffer;
        if (!resolutionItemsStr.empty()) {
            m_resolutionItems.clear();
            
            size_t start = 0, end = 0;
            while ((end = resolutionItemsStr.find(TEXT(","), start)) != std::wstring::npos) {
                std::wstring item = resolutionItemsStr.substr(start, end - start);
                if (!item.empty()) {
                    m_resolutionItems.push_back(item);
                }
                start = end + 1;
            }
            
            if (start < resolutionItemsStr.length()) {
                std::wstring item = resolutionItemsStr.substr(start);
                if (!item.empty()) {
                    m_resolutionItems.push_back(item);
                }
            }
        }
    }
}

void ConfigManager::SaveMenuConfig() {
    // 保存浮动窗口设置
    TCHAR buffer[32];
    _stprintf_s(buffer, _countof(buffer), TEXT("%d"), m_useFloatingWindow ? 1 : 0);
    WritePrivateProfileString(Constants::MENU_SECTION,
                            Constants::MENU_FLOATING,
                            buffer, m_configPath.c_str());
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

// 加载黑边模式配置
void ConfigManager::LoadLetterboxConfig() {
    TCHAR buffer[32];
    if (GetPrivateProfileString(Constants::LETTERBOX_SECTION,
                              Constants::LETTERBOX_ENABLED,
                              TEXT("0"), buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        m_letterboxEnabled = (_wtoi(buffer) != 0);
    }
}

// 保存黑边模式配置
void ConfigManager::SaveLetterboxConfig() {
    WritePrivateProfileString(Constants::LETTERBOX_SECTION,
                            Constants::LETTERBOX_ENABLED,
                            m_letterboxEnabled ? TEXT("1") : TEXT("0"),
                            m_configPath.c_str());
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

bool ConfigManager::AddCustomResolution(const std::wstring& resolution, std::vector<ResolutionPreset>& resolutions) {
    try {
        // 处理常见分辨率标识符
        if (resolution == TEXT("480P")) {
            resolutions.emplace_back(resolution, 720, 480);
            return true;
        } else if (resolution == TEXT("720P")) {
            resolutions.emplace_back(resolution, 1280, 720);
            return true;
        } else if (resolution == TEXT("1080P")) {
            resolutions.emplace_back(resolution, 1920, 1080);
            return true;
        } else if (resolution == TEXT("2K")) {
            resolutions.emplace_back(resolution, 2560, 1440);
            return true;
        } else if (resolution == TEXT("10K")) {
            resolutions.emplace_back(resolution, 10240, 4320);
            return true;
        } else if (resolution == TEXT("16K")) {
            resolutions.emplace_back(resolution, 15360, 8640);
            return true;
        }
        
        // 处理自定义分辨率格式 (例如 1920x1080)
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

// 获取默认的宽高比预设
std::vector<AspectRatio> ConfigManager::GetDefaultAspectRatios() {
    return {
        {TEXT("32:9"), 32.0/9.0},  // 超宽屏
        {TEXT("21:9"), 21.0/9.0},  // 宽屏
        {TEXT("16:9"), 16.0/9.0},  // 标准宽屏
        {TEXT("3:2"), 3.0/2.0},    // 传统显示器
        {TEXT("1:1"), 1.0},        // 正方形
        {TEXT("2:3"), 2.0/3.0},    // 竖屏
        {TEXT("9:16"), 9.0/16.0}   // 竖屏宽屏
    };
}

// 获取默认的分辨率预设
std::vector<ResolutionPreset> ConfigManager::GetDefaultResolutionPresets() {
    return {
        {TEXT("Default"), 0, 0},    // 默认选项，使用屏幕尺寸计算
        {TEXT("4K"), 3840, 2160},   // 8.3M pixels
        {TEXT("6K"), 5760, 3240},   // 18.7M pixels
        {TEXT("8K"), 7680, 4320},   // 33.2M pixels
        {TEXT("12K"), 11520, 6480}  // 74.6M pixels
    };
}

// 根据配置获取宽高比列表
ConfigLoadResult ConfigManager::GetAspectRatios(const LocalizedStrings& strings) {
    ConfigLoadResult result;
    
    // 1. 获取默认预设
    result.ratios = GetDefaultAspectRatios();
    
    // 2. 如果配置中没有自定义项，直接返回默认预设
    if (m_aspectRatioItems.empty()) {
        return result;
    }
    
    // 3. 处理配置中的自定义项
    std::vector<AspectRatio> configuredRatios;
    bool hasError = false;
    std::wstring errorDetails;
    
    for (const auto& item : m_aspectRatioItems) {
        // 检查是否是预定义项
        bool found = false;
        for (const auto& preset : result.ratios) {
            if (item == preset.name) {
                configuredRatios.push_back(preset);
                found = true;
                break;
            }
        }
        
        // 如果不是预定义项，尝试解析为自定义比例
        if (!found) {
            if (!AddCustomRatio(item, configuredRatios)) {
                hasError = true;
                errorDetails += item + L", ";
            }
        }
    }
    
    // 如果有有效的配置项，用它们替换默认预设
    if (!configuredRatios.empty()) {
        result.ratios = configuredRatios;
    }
    
    // 如果有错误，设置错误信息
    if (hasError) {
        if (errorDetails.length() >= 2) {
            errorDetails = errorDetails.substr(0, errorDetails.length() - 2);
        }
        result.success = false;
        result.errorDetails = strings.CONFIG_FORMAT_ERROR + L" " + errorDetails + L"\n" + strings.RATIO_FORMAT_EXAMPLE;
    }
    
    return result;
}

// 根据配置获取分辨率列表
ConfigLoadResult ConfigManager::GetResolutionPresets(const LocalizedStrings& strings) {
    ConfigLoadResult result;
    
    // 1. 获取默认预设
    result.resolutions = GetDefaultResolutionPresets();
    
    // 2. 如果配置中没有自定义项，直接返回默认预设
    if (m_resolutionItems.empty()) {
        return result;
    }
    
    // 3. 处理配置中的自定义项
    std::vector<ResolutionPreset> configuredResolutions;
    bool hasError = false;
    std::wstring errorDetails;
    
    for (const auto& item : m_resolutionItems) {
        // 检查是否是预定义项
        bool found = false;
        for (const auto& preset : result.resolutions) {
            if (item == preset.name) {
                configuredResolutions.push_back(preset);
                found = true;
                break;
            }
        }
        
        // 如果不是预定义项，尝试解析为自定义分辨率
        if (!found) {
            if (!AddCustomResolution(item, configuredResolutions)) {
                hasError = true;
                errorDetails += item + L", ";
            }
        }
    }
    
    // 如果有有效的配置项，用它们替换默认预设
    if (!configuredResolutions.empty()) {
        result.resolutions = configuredResolutions;
    }
    
    // 如果有错误，设置错误信息
    if (hasError) {
        if (errorDetails.length() >= 2) {
            errorDetails = errorDetails.substr(0, errorDetails.length() - 2);
        }
        result.success = false;
        result.errorDetails = strings.CONFIG_FORMAT_ERROR + L" " + errorDetails + L"\n" + strings.RESOLUTION_FORMAT_EXAMPLE;
    }
    
    return result;
}