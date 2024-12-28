#include "window_utils.hpp"
#include <algorithm>

// 窗口查找
HWND WindowUtils::FindGameWindow() {
    // 先尝试查找中文标题
    HWND hwnd = FindWindow(NULL, TEXT("无限暖暖  "));
    if (hwnd) return hwnd;
    
    // 如果找不到中文标题，尝试英文标题
    return FindWindow(NULL, TEXT("Infinity Nikki  "));
}

std::wstring WindowUtils::TrimRight(const std::wstring& str) {
    size_t end = str.find_last_not_of(L' ');
    return (end == std::wstring::npos) ? L"" : str.substr(0, end + 1);
}

bool WindowUtils::CompareWindowTitle(const std::wstring& title1, const std::wstring& title2) {
    return TrimRight(title1) == TrimRight(title2);
}

// 窗口操作
bool WindowUtils::ResizeWindow(HWND hwnd, int width, int height, bool topmost, bool taskbarLower) {
    if (!hwnd || !IsWindow(hwnd)) return false;

    // 获取窗口样式
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 如果是有边框窗口且需要超出屏幕尺寸，转换为无边框
    if ((style & WS_OVERLAPPEDWINDOW) && (width > screenWidth || height > screenHeight)) {
        style &= ~(WS_OVERLAPPEDWINDOW);
        style |= WS_POPUP;
        SetWindowLong(hwnd, GWL_STYLE, style);
    }

    // 调整窗口大小
    RECT rect = { 0, 0, width, height };
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);

    // 使用 rect 的 left 和 top 值来调整位置这些值通常是负数
    int totalWidth = rect.right - rect.left;
    int totalHeight = rect.bottom - rect.top;
    int borderOffsetX = rect.left;  // 左边框的偏移量（负值）
    int borderOffsetY = rect.top;   // 顶部边框的偏移量（负值）

    // 计算屏幕中心位置，考虑边框偏移
    int newLeft = (screenWidth - width) / 2 + borderOffsetX;
    int newTop = (screenHeight - height) / 2 + borderOffsetY;

    // 设置窗口置顶状态
    HWND insertAfter = topmost ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(hwnd, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // 设置新的窗口大小和位置
    bool success = SetWindowPos(hwnd, NULL, newLeft, newTop, totalWidth, totalHeight, 
                              SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;

    // 如果窗口调整成功且需要置底任务栏，则执行置底操作
    if (success && taskbarLower) {
        if (HWND taskbar = FindWindow(TEXT("Shell_TrayWnd"), NULL)) {
            SetWindowPos(taskbar, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    return success;
}

// 回调函数
BOOL CALLBACK WindowUtils::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    TCHAR className[256];
    TCHAR windowText[256];
    
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (!GetClassName(hwnd, className, 256)) return TRUE;
    if (!GetWindowText(hwnd, windowText, 256)) return TRUE;

    auto windows = reinterpret_cast<std::vector<std::pair<HWND, std::wstring>>*>(lParam);
    if (windowText[0] != '\0') {  // 只收集有标题的窗口
        std::pair<HWND, std::wstring> item(hwnd, windowText);
        windows->push_back(item);
    }

    return TRUE;
}

// 获取窗口列表
std::vector<std::pair<HWND, std::wstring>> WindowUtils::GetWindows() {
    std::vector<std::pair<HWND, std::wstring>> windows;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&windows));
    return windows;
}

// 分辨率计算
WindowUtils::Resolution WindowUtils::CalculateResolution(UINT64 totalPixels, double ratio) {
    // ratio 是宽高比，例如 9/16 = 0.5625
    int width = static_cast<int>(sqrt(totalPixels * ratio));
    int height = static_cast<int>(width / ratio);
    
    // 微调以确保总像素数准确
    if (static_cast<UINT64>(width) * height < totalPixels) {
        width++;
    }
    
    return WindowUtils::Resolution(width, height);
}

WindowUtils::Resolution WindowUtils::CalculateResolutionByScreen(double targetRatio) {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 方案1：使用屏幕宽度计算高度
    int height1 = static_cast<int>(screenWidth / targetRatio);
    
    // 方案2：使用屏幕高度计算宽度
    int width2 = static_cast<int>(screenHeight * targetRatio);
    
    // 选择不超出屏幕的方案
    if (width2 <= screenWidth) {
        // 如果基于高度计算的宽度不超出屏幕，使用方案2
        return WindowUtils::Resolution(width2, screenHeight);
    } else {
        // 否则使用方案1
        return WindowUtils::Resolution(screenWidth, height1);
    }
} 