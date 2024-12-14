#include "imgui.h"
#include "imgui_impl_win32.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>

// 简化版实现
static HWND g_hWnd = nullptr;

bool ImGui_ImplWin32_Init(void* hwnd)
{
    g_hWnd = (HWND)hwnd;
    return true;
}

void ImGui_ImplWin32_Shutdown()
{
    g_hWnd = nullptr;
}

void ImGui_ImplWin32_NewFrame()
{
    ImGuiIO& io = ImGui::GetIO();
    // 设置显示大小
    RECT rect;
    GetClientRect(g_hWnd, &rect);
    io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
}

// Win32消息处理
IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return 0;
}

void ImGui_ImplWin32_EnableDpiAwareness()
{
}

float ImGui_ImplWin32_GetDpiScaleForHwnd(void* hwnd)
{
    return 1.0f;
}

float ImGui_ImplWin32_GetDpiScaleForMonitor(void* monitor)
{
    return 1.0f;
} 