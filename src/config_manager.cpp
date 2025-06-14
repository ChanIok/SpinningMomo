#include "config_manager.hpp"
#include <shellapi.h>
#include <shlwapi.h>
#include <string> // Include for std::stoi/stoul

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
    // WritePrivateProfileStringW(Constants::LOGGER_SECTION, Constants::LOGGER_LEVEL, L"INFO", m_configPath.c_str());
    WritePrivateProfileStringW(Constants::WINDOW_SECTION, Constants::WINDOW_TITLE, L"", m_configPath.c_str());
    WritePrivateProfileStringW(Constants::HOTKEY_SECTION, Constants::HOTKEY_MODIFIERS, L"3", m_configPath.c_str());
    WritePrivateProfileStringW(Constants::HOTKEY_SECTION, Constants::HOTKEY_KEY, L"82", m_configPath.c_str());
    WritePrivateProfileStringW(Constants::MENU_SECTION, Constants::MENU_FLOATING, L"1", m_configPath.c_str());
    WritePrivateProfileStringW(Constants::SCREENSHOT_SECTION, Constants::SCREENSHOT_PATH, L"", m_configPath.c_str());
    WritePrivateProfileStringW(Constants::MENU_SECTION, Constants::MENU_ITEMS, 
                                L"CaptureWindow,OpenScreenshot,PreviewWindow,OverlayWindow,LetterboxWindow,Reset,Close,Exit", 
                                m_configPath.c_str());
        
    // 添加默认的宽高比和分辨率配置
    WritePrivateProfileStringW(Constants::MENU_SECTION, Constants::ASPECT_RATIO_ITEMS, 
                                L"32:9,21:9,16:9,3:2,1:1,3:4,2:3,9:16", 
                                m_configPath.c_str());
    WritePrivateProfileStringW(Constants::MENU_SECTION, Constants::RESOLUTION_ITEMS, 
                                L"Default,1080P,2K,4K,6K,8K,12K", 
                                m_configPath.c_str());
        
    // 添加默认的黑边模式配置（默认为禁用）
    WritePrivateProfileStringW(Constants::LETTERBOX_SECTION, Constants::LETTERBOX_ENABLED, 
                                L"0", 
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
    // LoadLogConfig();
    LoadLetterboxConfig();
}

// void ConfigManager::LoadLogConfig() {
//     wchar_t buffer[32];
//     if (GetPrivateProfileStringW(Constants::LOGGER_SECTION, 
//                                Constants::LOGGER_LEVEL,
//                                L"", 
//                                buffer, 
//                                _countof(buffer),
//                                m_configPath.c_str()) > 0) {
//         std::wstring logLevelStr = buffer;
        
//         if (logLevelStr == L"DEBUG") {
//             m_logLevel = LogLevel::DEBUG;
//         } else if (logLevelStr == L"INFO") {
//             m_logLevel = LogLevel::INFO;
//         } else if (logLevelStr == L"ERROR") {
//             m_logLevel = LogLevel::ERR;
//         }
//     }
// }

void ConfigManager::LoadHotkeyConfig() {
    wchar_t buffer[32];
    // 读取修饰键
    if (GetPrivateProfileStringW(Constants::HOTKEY_SECTION, 
                              Constants::HOTKEY_MODIFIERS,
                              L"", buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        try { m_hotkeyModifiers = std::stoul(buffer); } catch (...) {}
    }

    // 读取主键
    if (GetPrivateProfileStringW(Constants::HOTKEY_SECTION,
                              Constants::HOTKEY_KEY,
                              L"", buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        try { m_hotkeyKey = std::stoul(buffer); } catch (...) {}
    }
}

void ConfigManager::SaveHotkeyConfig() {
    wchar_t buffer[32];
    // 保存修饰键
    swprintf_s(buffer, _countof(buffer), L"%u", m_hotkeyModifiers);
    WritePrivateProfileStringW(Constants::HOTKEY_SECTION,
                            Constants::HOTKEY_MODIFIERS,
                            buffer, m_configPath.c_str());

    // 保存主键
    swprintf_s(buffer, _countof(buffer), L"%u", m_hotkeyKey);
    WritePrivateProfileStringW(Constants::HOTKEY_SECTION,
                            Constants::HOTKEY_KEY,
                            buffer, m_configPath.c_str());
}

void ConfigManager::LoadWindowConfig() {
    wchar_t buffer[256];
    if (GetPrivateProfileStringW(Constants::WINDOW_SECTION,
                              Constants::WINDOW_TITLE,
                              L"", buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        m_windowTitle = buffer; // 直接赋值，期望引号已被去除
    } else {
        m_windowTitle.clear(); // 确保如果读取失败或为空，则为空
    }
}

void ConfigManager::SaveWindowConfig() {
    if (!m_windowTitle.empty()) {
        // 用双引号包裹标题
        std::wstring titleToWrite = L"\"" + m_windowTitle + L"\"";
        WritePrivateProfileStringW(Constants::WINDOW_SECTION,
                                Constants::WINDOW_TITLE,
                                titleToWrite.c_str(), //写入带引号的标题
                                m_configPath.c_str());
    }
}

void ConfigManager::LoadLanguageConfig() {
    wchar_t buffer[32];
    if (GetPrivateProfileStringW(Constants::LANG_SECTION,
                              Constants::LANG_CURRENT,
                              L"", buffer, _countof(buffer),
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
    WritePrivateProfileStringW(Constants::LANG_SECTION,
                            Constants::LANG_CURRENT,
                            m_language.c_str(),
                            m_configPath.c_str());
}

void ConfigManager::LoadTaskbarConfig() {
    wchar_t buffer[32];
    if (GetPrivateProfileStringW(Constants::TASKBAR_SECTION,
                              Constants::TASKBAR_AUTOHIDE,
                              L"0", buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        try { m_taskbarAutoHide = (std::stoi(buffer) != 0); } catch (...) {}
    }
    
    if (GetPrivateProfileStringW(Constants::TASKBAR_SECTION,
                              Constants::TASKBAR_LOWER,
                              L"1", buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        try { m_taskbarLower = (std::stoi(buffer) != 0); } catch (...) {}
    }
}

void ConfigManager::SaveTaskbarConfig() {
    WritePrivateProfileStringW(Constants::TASKBAR_SECTION,
                            Constants::TASKBAR_AUTOHIDE,
                            m_taskbarAutoHide ? L"1" : L"0",
                            m_configPath.c_str());
                            
    WritePrivateProfileStringW(Constants::TASKBAR_SECTION,
                            Constants::TASKBAR_LOWER,
                            m_taskbarLower ? L"1" : L"0",
                            m_configPath.c_str());
}

void ConfigManager::LoadMenuConfig() {
    wchar_t buffer[1024];
    
    // 读取浮动窗口设置
    if (GetPrivateProfileStringW(Constants::MENU_SECTION,
                              Constants::MENU_FLOATING,
                              L"1", buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        try { m_useFloatingWindow = (std::stoi(buffer) != 0); } catch (...) {}
    }
    
    // 读取菜单项显示配置
    if (GetPrivateProfileStringW(Constants::MENU_SECTION,
                              Constants::MENU_ITEMS,
                              L"", buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        std::wstring itemsStr = buffer;
        if (!itemsStr.empty()) {
            m_menuItemsToShow.clear();
            
            size_t start = 0, end = 0;
            while ((end = itemsStr.find(L",", start)) != std::wstring::npos) {
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
    if (GetPrivateProfileStringW(Constants::MENU_SECTION,
                              Constants::ASPECT_RATIO_ITEMS,
                              L"", buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        std::wstring ratioItemsStr = buffer;
        if (!ratioItemsStr.empty()) {
            m_aspectRatioItems.clear();
            
            size_t start = 0, end = 0;
            while ((end = ratioItemsStr.find(L",", start)) != std::wstring::npos) {
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
    if (GetPrivateProfileStringW(Constants::MENU_SECTION,
                              Constants::RESOLUTION_ITEMS,
                              L"", buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        std::wstring resolutionItemsStr = buffer;
        if (!resolutionItemsStr.empty()) {
            m_resolutionItems.clear();
            
            size_t start = 0, end = 0;
            while ((end = resolutionItemsStr.find(L",", start)) != std::wstring::npos) {
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
    wchar_t buffer[32];
    swprintf_s(buffer, _countof(buffer), L"%d", m_useFloatingWindow ? 1 : 0);
    WritePrivateProfileStringW(Constants::MENU_SECTION,
                            Constants::MENU_FLOATING,
                            buffer, m_configPath.c_str());
}

void ConfigManager::LoadGameAlbumConfig() {
    wchar_t buffer[MAX_PATH];
    if (GetPrivateProfileStringW(Constants::SCREENSHOT_SECTION,
                              Constants::SCREENSHOT_PATH,
                              L"", buffer, MAX_PATH,
                              m_configPath.c_str()) > 0) {
        m_gameAlbumPath = buffer;
    }
}

void ConfigManager::SaveGameAlbumConfig() {
    if (!m_gameAlbumPath.empty()) {
        WritePrivateProfileStringW(Constants::SCREENSHOT_SECTION,
                                Constants::SCREENSHOT_PATH,
                                m_gameAlbumPath.c_str(),
                                m_configPath.c_str());
    }
}

// 加载黑边模式配置
void ConfigManager::LoadLetterboxConfig() {
    wchar_t buffer[32];
    if (GetPrivateProfileStringW(Constants::LETTERBOX_SECTION,
                              Constants::LETTERBOX_ENABLED,
                              L"0", buffer, _countof(buffer),
                              m_configPath.c_str()) > 0) {
        try { m_letterboxEnabled = (std::stoi(buffer) != 0); } catch (...) {}
    }
}

// 保存黑边模式配置
void ConfigManager::SaveLetterboxConfig() {
    WritePrivateProfileStringW(Constants::LETTERBOX_SECTION,
                            Constants::LETTERBOX_ENABLED,
                            m_letterboxEnabled ? L"1" : L"0",
                            m_configPath.c_str());
}

bool ConfigManager::AddCustomRatio(const std::wstring& ratio, std::vector<AspectRatio>& ratios) {
    size_t colonPos = ratio.find(L":");
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
        if (resolution == L"480P") {
            resolutions.emplace_back(resolution, 720, 480);
            return true;
        } else if (resolution == L"720P") {
            resolutions.emplace_back(resolution, 1280, 720);
            return true;
        } else if (resolution == L"1080P") {
            resolutions.emplace_back(resolution, 1920, 1080);
            return true;
        } else if (resolution == L"2K") {
            resolutions.emplace_back(resolution, 2560, 1440);
            return true;
        } else if (resolution == L"5K") {
            resolutions.emplace_back(resolution, 5120, 2880);
            return true;
        } else if (resolution == L"10K") {
            resolutions.emplace_back(resolution, 10240, 4320);
            return true;
        } else if (resolution == L"16K") {
            resolutions.emplace_back(resolution, 15360, 8640);
            return true;
        }
        
        // 处理自定义分辨率格式 (例如 1920x1080)
        size_t xPos = resolution.find(L"x");
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
        {L"32:9", 32.0/9.0},  // 超宽屏
        {L"21:9", 21.0/9.0},  // 宽屏
        {L"16:9", 16.0/9.0},  // 标准宽屏
        {L"3:2", 3.0/2.0},    // 传统显示器
        {L"1:1", 1.0},        // 正方形
        {L"2:3", 2.0/3.0},    // 竖屏
        {L"9:16", 9.0/16.0}   // 竖屏宽屏
    };
}

// 获取默认的分辨率预设
std::vector<ResolutionPreset> ConfigManager::GetDefaultResolutionPresets() {
    return {
        {L"Default", 0, 0},    // 默认选项，使用屏幕尺寸计算
        {L"4K", 3840, 2160},   // 8.3M pixels
        {L"6K", 5760, 3240},   // 18.7M pixels
        {L"8K", 7680, 4320},   // 33.2M pixels
        {L"12K", 11520, 6480}  // 74.6M pixels
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
