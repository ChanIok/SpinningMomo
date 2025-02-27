#pragma once
#include "win_config.hpp"
#include "constants.hpp"
#include <string>
#include <vector>

// 配置加载结果
struct ConfigLoadResult {
    bool success;
    std::vector<AspectRatio> ratios;
    std::vector<ResolutionPreset> resolutions;
    std::wstring errorDetails;
    
    ConfigLoadResult() : success(true) {}
    ConfigLoadResult(const std::wstring& error) : success(false), errorDetails(error) {}
};

class ConfigManager {
public:
    ConfigManager();
    
    // 初始化配置文件路径
    void Initialize();
    
    // 配置加载
    void LoadAllConfigs();
    void LoadHotkeyConfig();
    void LoadWindowConfig();
    void LoadLanguageConfig();
    void LoadTaskbarConfig();
    void LoadMenuConfig();
    void LoadGameAlbumConfig();
    
    // 配置保存
    void SaveHotkeyConfig();
    void SaveWindowConfig();
    void SaveLanguageConfig();
    void SaveTaskbarConfig();
    void SaveMenuConfig();
    void SaveGameAlbumConfig();

    // 根据配置构建宽高比和分辨率列表
    ConfigLoadResult GetAspectRatios(const LocalizedStrings& strings);
    ConfigLoadResult GetResolutionPresets(const LocalizedStrings& strings);

    // Getters
    const std::wstring& GetConfigPath() const { return m_configPath; }
    const std::wstring& GetWindowTitle() const { return m_windowTitle; }
    const std::wstring& GetGameAlbumPath() const { return m_gameAlbumPath; }
    const std::wstring& GetLanguage() const { return m_language; }
    UINT GetHotkeyModifiers() const { return m_hotkeyModifiers; }
    UINT GetHotkeyKey() const { return m_hotkeyKey; }
    bool GetTaskbarAutoHide() const { return m_taskbarAutoHide; }
    bool GetTaskbarLower() const { return m_taskbarLower; }
    bool GetUseFloatingWindow() const { return m_useFloatingWindow; }
    const std::vector<std::wstring>& GetMenuItemsToShow() const { return m_menuItemsToShow; }
    const std::vector<std::wstring>& GetAspectRatioItems() const { return m_aspectRatioItems; }
    const std::vector<std::wstring>& GetResolutionItems() const { return m_resolutionItems; }
    
    // Setters
    void SetWindowTitle(const std::wstring& title) { m_windowTitle = title; }
    void SetGameAlbumPath(const std::wstring& path) { m_gameAlbumPath = path; }
    void SetLanguage(const std::wstring& lang) { m_language = lang; }
    void SetHotkeyModifiers(UINT modifiers) { m_hotkeyModifiers = modifiers; }
    void SetHotkeyKey(UINT key) { m_hotkeyKey = key; }
    void SetTaskbarAutoHide(bool autoHide) { m_taskbarAutoHide = autoHide; }
    void SetTaskbarLower(bool lower) { m_taskbarLower = lower; }
    void SetUseFloatingWindow(bool use) { m_useFloatingWindow = use; }
    void SetMenuItemsToShow(const std::vector<std::wstring>& items) { m_menuItemsToShow = items; }
    void SetAspectRatioItems(const std::vector<std::wstring>& items) { m_aspectRatioItems = items; }
    void SetResolutionItems(const std::vector<std::wstring>& items) { m_resolutionItems = items; }

private:
    // 获取默认的宽高比和分辨率预设
    std::vector<AspectRatio> GetDefaultAspectRatios();
    std::vector<ResolutionPreset> GetDefaultResolutionPresets();
    
    bool AddCustomRatio(const std::wstring& ratio, std::vector<AspectRatio>& ratios);
    bool AddCustomResolution(const std::wstring& resolution, std::vector<ResolutionPreset>& resolutions);

    std::wstring m_configPath;
    std::wstring m_windowTitle;
    std::wstring m_gameAlbumPath;
    std::wstring m_language;
    UINT m_hotkeyModifiers = MOD_CONTROL | MOD_ALT;
    UINT m_hotkeyKey = 'R';
    bool m_taskbarAutoHide = false;
    bool m_taskbarLower = true;
    bool m_useFloatingWindow = true;
    std::vector<std::wstring> m_menuItemsToShow;  // 要显示的菜单项类型
    std::vector<std::wstring> m_aspectRatioItems; // 要显示的宽高比项
    std::vector<std::wstring> m_resolutionItems;  // 要显示的分辨率项
}; 