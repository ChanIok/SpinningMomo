#pragma once
#include "win_config.hpp"
#include <shellapi.h>
#include <strsafe.h>
#include <vector>
#include <string>
#include <dwmapi.h>
#include "constants.hpp"

#define IDI_ICON1 101  // 从main_old.cpp迁移图标ID定义

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
        bool topmostEnabled,
        bool taskbarAutoHide,
        bool taskbarLower,
        bool notifyEnabled,
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
        bool topmostEnabled,
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
        bool topmostEnabled,
        bool taskbarAutoHide,
        bool taskbarLower,
        bool notifyEnabled,
        bool useFloatingWindow,
        bool isFloatingWindowVisible,
        bool previewEnabled,
        const LocalizedStrings& strings);
};

// 菜单窗口类
class MenuWindow {
public:
    // 列表项类型
    enum class ItemType {
        Ratio,
        Resolution,
        TaskbarAutoHide,
        PreviewWindow,
        Reset,
        Close
    };

    // 列表项结构
    struct MenuItem {
        std::wstring text;
        ItemType type;
        int index;  // 在对应类型中的索引
    };

    MenuWindow(HINSTANCE hInstance);
    
    bool Create(HWND parent, 
               std::vector<AspectRatio>& ratios,           // 使用引用
               std::vector<ResolutionPreset>& resolutions, // 使用引用
               const LocalizedStrings& strings,            // 字符串只读，保持const
               size_t currentRatioIndex,                   // 添加初始比例索引
               size_t currentResolutionIndex,              // 添加初始分辨率索引
               bool taskbarAutoHide,                       // 添加初始任务栏自动隐藏状态
               bool previewEnabled);                       // 添加初始预览窗口状态
    
    void Show();
    void Hide();
    bool IsVisible() const;
    void ToggleVisibility();
    void SetCurrentRatio(size_t index);
    void SetCurrentResolution(size_t index);
    void SetTaskbarAutoHide(bool enabled);
    void SetPreviewEnabled(bool enabled);  // 添加设置预览窗口状态的函数
    void UpdateMenuItems(const LocalizedStrings& strings, bool forceRedraw = true);
    HWND GetHwnd() const;

private:
    static const TCHAR* MENU_WINDOW_CLASS;  // 只声明
    // 基础尺寸（96 DPI）
    static constexpr int BASE_ITEM_HEIGHT = 24;        // 列表项高度
    static constexpr int BASE_TITLE_HEIGHT = 26;       // 标题栏高度
    static constexpr int BASE_SEPARATOR_HEIGHT = 1;    // 分隔线高度
    static constexpr int BASE_FONT_SIZE = 12;          // 字体大小
    static constexpr int BASE_TEXT_PADDING = 12;       // 文本内边距
    static constexpr int BASE_INDICATOR_WIDTH = 3;     // 指示器宽度
    static constexpr int BASE_RATIO_INDICATOR_WIDTH = 4;  // 比例组指示器宽度
    
    // 添加列宽常量
    static constexpr int BASE_RATIO_COLUMN_WIDTH = 60;      // 比例列宽度
    static constexpr int BASE_RESOLUTION_COLUMN_WIDTH = 120; // 分辨率列宽度
    static constexpr int BASE_SETTINGS_COLUMN_WIDTH = 120;   // 设置列宽度

    // DPI相关的尺寸变量
    UINT m_dpi = 96;
    int m_itemHeight = BASE_ITEM_HEIGHT;
    int m_titleHeight = BASE_TITLE_HEIGHT;
    int m_separatorHeight = BASE_SEPARATOR_HEIGHT;
    int m_fontSize = BASE_FONT_SIZE;
    int m_textPadding = BASE_TEXT_PADDING;
    int m_indicatorWidth = BASE_INDICATOR_WIDTH;
    int m_ratioIndicatorWidth = BASE_RATIO_INDICATOR_WIDTH;
    
    // 添加列宽变量
    int m_ratioColumnWidth = BASE_RATIO_COLUMN_WIDTH;
    int m_resolutionColumnWidth = BASE_RESOLUTION_COLUMN_WIDTH;
    int m_settingsColumnWidth = BASE_SETTINGS_COLUMN_WIDTH;

    HWND m_hwnd = NULL;
    HWND m_hwndParent = NULL;
    HINSTANCE m_hInstance = NULL;
    int m_hoverIndex = -1;                       // 当前鼠标停项
    size_t m_currentRatioIndex = SIZE_MAX;       // 当前选中的比例索引
    size_t m_currentResolutionIndex = SIZE_MAX;  // 当前选中的分辨率索引
    bool m_taskbarAutoHide = false;              // 任务栏自动隐藏状态
    bool m_previewEnabled = false;               // 预览窗口状态
    std::vector<AspectRatio>* m_ratioItems = nullptr;       // 使用指针
    std::vector<ResolutionPreset>* m_resolutionItems = nullptr;  // 使用指针
    std::vector<MenuItem> m_items;               // 所有列表项
    const LocalizedStrings* m_strings = nullptr;            // 字符串只读

    // 私有方法
    void InitializeItems(const LocalizedStrings& strings);
    void RegisterWindowClass();
    static LRESULT CALLBACK MenuWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnPaint(HDC hdc);
    void OnMouseMove(int x, int y);
    void OnMouseLeave();
    void OnLButtonDown(int x, int y);
    void UpdateDpiDependentResources();
    int CalculateWindowHeight();
    int GetItemIndexFromPoint(int x, int y);
}; 