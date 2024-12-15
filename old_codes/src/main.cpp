#include "app.h"
#include <windows.h>
#include <iostream>
#include "capture_test.h"

// 设置子系统为Windows
#pragma comment(linker, "/subsystem:windows")

// 添加SAL批注
int WINAPI wWinMain(_In_ HINSTANCE hInstance,
                    _In_opt_ HINSTANCE hPrevInstance,
                    _In_ LPWSTR lpCmdLine,
                    _In_ int nCmdShow)
{
    // 启用控制台输出用于调试
    #ifdef _DEBUG
    AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    #endif

    try {
        App& app = App::GetInstance();
        if (!app.Initialize()) {
            MessageBoxW(NULL, L"Failed to initialize application", L"Error", MB_OK | MB_ICONERROR);
            return 1;
        }

        app.Run();
        app.Cleanup();
    }
    catch (const std::exception& e) {
        // 将std::string转换为std::wstring
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, NULL, 0);
        std::wstring wstrError(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, &wstrError[0], size_needed);
        
        MessageBoxW(NULL, wstrError.c_str(), L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    return 0;
} 