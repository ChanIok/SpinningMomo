#pragma once
#include "win_config.hpp"
#include <shellapi.h>
#include <strsafe.h>
#include <vector>
#include <string>
#include <dwmapi.h>
#include "constants.hpp"

#define IDI_ICON1 101

// 前向声明
struct AspectRatio;
struct ResolutionPreset;

// 系统托盘图标管理类
class TrayIcon {
public:
    TrayIcon(HWND hwnd);
    ~TrayIcon();
    
    bool Create();
    void ShowBalloon(const TCHAR* title, const TCHAR* message);
    void UpdateTip(const TCHAR* tip);

    // 新增：显示上下文菜单
    void ShowContextMenu(
        const std::vector<std::pair<HWND, std::wstring>>& windows,
        const std::wstring& currentTitle,
        const std::vector<AspectRatio>& ratios,
        size_t currentRatioIndex,
        const std::vector<ResolutionPreset>& resolutions,
        size_t currentResolutionIndex,
        const LocalizedStrings& strings,
        bool taskbarAutoHide,
        bool taskbarLower,
        const std::wstring& language,
        bool useFloatingWindow,
        bool isFloatingWindowVisible,
        bool previewEnabled);
    // 新增：显示快捷菜单
    void ShowQuickMenu(
        const POINT& pt,
        const std::vector<AspectRatio>& ratios,
        size_t currentRatioIndex,
        const std::vector<ResolutionPreset>& resolutions,
        size_t currentResolutionIndex,
        const LocalizedStrings& strings,
        bool taskbarAutoHide,
        bool previewEnabled);
        
private:
    HWND m_hwnd;
    NOTIFYICONDATA m_nid{};

    // 新增：私有辅助方法
    HMENU CreateWindowSelectionSubmenu(
        const std::vector<std::pair<HWND, std::wstring>>& windows,
        const std::wstring& currentTitle,
        const LocalizedStrings& strings);
        
    HMENU CreateRatioSubmenu(
        const std::vector<AspectRatio>& ratios,
        size_t currentRatioIndex,
        const LocalizedStrings& strings);
        
    HMENU CreateResolutionSubmenu(
        const std::vector<ResolutionPreset>& resolutions,
        size_t currentResolutionIndex,
        const LocalizedStrings& strings);
        
    HMENU CreateLanguageSubmenu(
        const std::wstring& currentLanguage,
        const LocalizedStrings& strings);
        
    void AddSettingsItems(
        HMENU hMenu,
        bool taskbarAutoHide,
        bool taskbarLower,
        bool useFloatingWindow,
        bool isFloatingWindowVisible,
        bool previewEnabled,
        const LocalizedStrings& strings);
}; 