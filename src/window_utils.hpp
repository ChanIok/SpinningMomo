#pragma once
#include "win_config.hpp"
#include <string>
#include <vector>

class WindowUtils {
public:
    // 分辨率结构体
    struct Resolution {
        int width;
        int height;
        UINT64 totalPixels;

        Resolution(int w = 0, int h = 0) 
            : width(w), height(h), totalPixels(static_cast<UINT64>(w) * h) {}
    };

    // 窗口查找
    static HWND FindGameWindow();
    static std::vector<std::pair<HWND, std::wstring>> GetWindows();
    static bool CompareWindowTitle(const std::wstring& title1, const std::wstring& title2);
    
    // 窗口操作
    static bool ResizeWindow(HWND hwnd, int width, int height, bool topmost = false, bool taskbarLower = true);
    
    // 辅助方法
    static std::wstring TrimRight(const std::wstring& str);

    // 分辨率计算
    static Resolution CalculateResolution(UINT64 totalPixels, double ratio);
    static Resolution CalculateResolutionByScreen(double targetRatio);

private:
    // 回调函数
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
}; 