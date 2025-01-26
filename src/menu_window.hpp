#pragma once
#include "win_config.hpp"
#include <shellapi.h>
#include <strsafe.h>
#include <vector>
#include <string>
#include <dwmapi.h>
#include "constants.hpp"
#include "parameter_types.hpp"  // 添加参数类型头文件
#include "message_center.hpp"  // 添加消息中心头文件

// 前向声明
struct AspectRatio;
struct ResolutionPreset;

// 菜单窗口类
class MenuWindow {
public:
    // 列表项类型
    enum class ItemType {
        Ratio,
        Resolution,
        CaptureWindow,
        OpenScreenshot,
        PreviewWindow,
        Reset,
        ParameterTracking,  // 添加参数追踪类型
        Close
    };

    // 列表项结构
    struct MenuItem {
        std::wstring text;
        ItemType type;
        int index;  // 在对应类型中的索引
    };

    MenuWindow(HINSTANCE hInstance);
    ~MenuWindow() {
        // 取消订阅消息
        MessageCenter::Instance().Unsubscribe(MessageType::ParameterUpdated, this);
    }
    
    bool Create(HWND parent, 
               std::vector<AspectRatio>& ratios,           // 使用引用
               std::vector<ResolutionPreset>& resolutions, // 使用引用
               const LocalizedStrings& strings,            // 字符串只读，保持const
               size_t currentRatioIndex,                   // 添加初始比例索引
               size_t currentResolutionIndex,              // 添加初始分辨率索引
               bool previewEnabled);                       // 添加初始预览窗口状态
    
    void Show();
    void Hide();
    bool IsVisible() const;
    void ToggleVisibility();
    void SetCurrentRatio(size_t index);
    void SetCurrentResolution(size_t index);
    void SetPreviewEnabled(bool enabled);  // 添加设置预览窗口状态的函数
    void UpdateMenuItems(const LocalizedStrings& strings, bool forceRedraw = true);
    HWND GetHwnd() const;
    void UpdateParameters(const Parameters& params);  // 添加参数更新方法
    void SetParameterTrackingEnabled(bool enabled);
    bool IsParameterTrackingEnabled() const { return m_parameterTrackingEnabled; }

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

    // 参数区域相关常量
    static constexpr int BASE_PARAMETER_ITEM_HEIGHT = 24;    // 每个参数项基础高度
    static constexpr int BASE_PARAMETER_NAME_WIDTH = 70;     // 参数名称基础宽度
    static constexpr int BASE_PARAMETER_VALUE_WIDTH = 50;    // 参数值基础宽度
    static constexpr int PARAMETER_ITEMS_PER_COLUMN = 5;     // 每列参数项数量
    static constexpr int PARAMETER_AREA_PADDING = 5;         // 参数区域上下边距

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

    // 参数区域相关变量
    int m_parameterItemHeight = BASE_PARAMETER_ITEM_HEIGHT;
    int m_parameterNameWidth = BASE_PARAMETER_NAME_WIDTH;
    int m_parameterValueWidth = BASE_PARAMETER_VALUE_WIDTH;
    RECT m_parameterRect;  // 参数区域矩形
    Parameters m_currentParams;  // 当前参数值

    HWND m_hwnd = NULL;
    HWND m_hwndParent = NULL;
    HINSTANCE m_hInstance = NULL;
    int m_hoverIndex = -1;                       // 当前鼠标停项
    size_t m_currentRatioIndex = SIZE_MAX;       // 当前选中的比例索引
    size_t m_currentResolutionIndex = SIZE_MAX;  // 当前选中的分辨率索引
    bool m_previewEnabled = false;               // 预览窗口状态
    std::vector<AspectRatio>* m_ratioItems = nullptr;       // 使用指针
    std::vector<ResolutionPreset>* m_resolutionItems = nullptr;  // 使用指针
    std::vector<MenuItem> m_items;               // 所有列表项
    const LocalizedStrings* m_strings = nullptr;            // 字符串只读
    bool m_parameterTrackingEnabled = false;  // 参数追踪状态

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

    // 添加参数区域相关方法
    void DrawParameterArea(HDC hdc);
    void DrawParameterItem(HDC hdc, const ParameterValue& value, 
                         const std::wstring& name, const RECT& rect,
                         ParameterType type);
    bool IsPercentageParameter(ParameterType type) const;
    void UpdateParameterRect();
}; 