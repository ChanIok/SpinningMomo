#pragma once

#include <windows.h>

namespace Vendor::Windows {

// Types
using BOOL = ::BOOL;
using DWORD = ::DWORD;
using UINT = ::UINT;
using LANGID = ::LANGID;
using WPARAM = ::WPARAM;
using LPARAM = ::LPARAM;
using LRESULT = ::LRESULT;
using HWND = ::HWND;
using HHOOK = ::HHOOK;
using HINSTANCE = ::HINSTANCE;
using LPWSTR = ::LPWSTR;
using LPCWSTR = ::LPCWSTR;
using POINT = ::POINT;
using RECT = ::RECT;
using SIZE = ::SIZE;
using MSG = ::MSG;
using OSVERSIONINFOEXW = ::OSVERSIONINFOEXW;
using LPOSVERSIONINFOW = ::LPOSVERSIONINFOW;
using WIN32_FILE_ATTRIBUTE_DATA = ::WIN32_FILE_ATTRIBUTE_DATA;
using FILETIME = ::FILETIME;
using ULARGE_INTEGER = ::ULARGE_INTEGER;
using LPVOID = ::LPVOID;
using GET_FILEEX_INFO_LEVELS = ::GET_FILEEX_INFO_LEVELS;
using HANDLE = ::HANDLE;
using HRESULT = ::HRESULT;

// Constants
constexpr UINT kMB_ICONERROR = MB_ICONERROR;
constexpr UINT kWM_USER = WM_USER;
constexpr UINT kWM_QUIT = WM_QUIT;
constexpr UINT kPM_REMOVE = PM_REMOVE;
constexpr DWORD kQS_ALLINPUT = QS_ALLINPUT;
constexpr DWORD kMWMO_INPUTAVAILABLE = MWMO_INPUTAVAILABLE;
constexpr UINT kMOD_CONTROL = MOD_CONTROL;
constexpr UINT kMOD_ALT = MOD_ALT;
constexpr UINT kSWP_NOZORDER = SWP_NOZORDER;
constexpr UINT kSWP_NOACTIVATE = SWP_NOACTIVATE;
constexpr GET_FILEEX_INFO_LEVELS kGetFileExInfoStandard = ::GetFileExInfoStandard;
constexpr DWORD kMAX_PATH = MAX_PATH;
constexpr DWORD kERROR_CANCELLED = ERROR_CANCELLED;

// System metrics
inline auto GetSystemMetrics(int nIndex) -> int { return ::GetSystemMetrics(nIndex); }
inline auto GetScreenWidth() -> int { return ::GetSystemMetrics(SM_CXSCREEN); }
inline auto GetScreenHeight() -> int { return ::GetSystemMetrics(SM_CYSCREEN); }
inline auto GetUserDefaultUILanguage() -> LANGID { return ::GetUserDefaultUILanguage(); }

// Message box
inline auto MessageBoxW(HWND hWnd, const wchar_t* lpText, const wchar_t* lpCaption, UINT uType)
    -> int {
  return ::MessageBoxW(hWnd, lpText, lpCaption, uType);
}
inline auto MessageBoxA(HWND hWnd, const char* lpText, const char* lpCaption, UINT uType) -> int {
  return ::MessageBoxA(hWnd, lpText, lpCaption, uType);
}

// Message loop
inline auto GetWindowMessage(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
    -> BOOL {
  return ::GetMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
}
inline auto PeekMessageW(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax,
                         UINT wRemoveMsg) -> BOOL {
  return ::PeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
}
inline auto TranslateWindowMessage(const MSG* lpMsg) -> BOOL { return ::TranslateMessage(lpMsg); }
inline auto DispatchWindowMessageW(const MSG* lpMsg) -> LRESULT {
  return ::DispatchMessageW(lpMsg);
}

// Window operations
inline auto GetForegroundWindow() -> HWND { return ::GetForegroundWindow(); }
inline auto IsWindow(HWND hWnd) -> BOOL { return ::IsWindow(hWnd); }
inline auto GetWindowRect(HWND hWnd, RECT* lpRect) -> BOOL { return ::GetWindowRect(hWnd, lpRect); }
inline auto SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int x, int y, int cx, int cy, UINT uFlags)
    -> BOOL {
  return ::SetWindowPos(hWnd, hWndInsertAfter, x, y, cx, cy, uFlags);
}
inline auto InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase) -> BOOL {
  return ::InvalidateRect(hWnd, lpRect, bErase);
}
inline auto PostMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) -> BOOL {
  return ::PostMessageW(hWnd, Msg, wParam, lParam);
}

// Application control
inline auto PostQuitMessage(int nExitCode) -> void { ::PostQuitMessage(nExitCode); }
inline auto GetCurrentProcessId() -> DWORD { return ::GetCurrentProcessId(); }

// Synchronization
inline auto MsgWaitForMultipleObjectsEx(DWORD nCount, const HANDLE* pHandles, DWORD dwMilliseconds,
                                        DWORD dwWakeMask, DWORD dwFlags) -> DWORD {
  return ::MsgWaitForMultipleObjectsEx(nCount, pHandles, dwMilliseconds, dwWakeMask, dwFlags);
}

// File operations
inline auto GetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId,
                                 LPVOID lpFileInformation) -> BOOL {
  return ::GetFileAttributesExW(lpFileName, fInfoLevelId, lpFileInformation);
}

// INI configuration
inline auto GetPrivateProfileStringW(LPCWSTR lpAppName, LPCWSTR lpKeyName, LPCWSTR lpDefault,
                                     LPWSTR lpReturnedString, DWORD nSize, LPCWSTR lpFileName)
    -> DWORD {
  return ::GetPrivateProfileStringW(lpAppName, lpKeyName, lpDefault, lpReturnedString, nSize,
                                    lpFileName);
}

// Error handling
inline auto GetLastError() -> DWORD { return ::GetLastError(); }

// Handle operations
inline auto CloseHandle(HANDLE hObject) -> BOOL { return ::CloseHandle(hObject); }

// HRESULT handling functions (replacing macros)
constexpr auto _HRESULT_FROM_WIN32(DWORD x) -> HRESULT {
  return static_cast<HRESULT>(x) <= 0
             ? static_cast<HRESULT>(x)
             : static_cast<HRESULT>((x & 0x0000FFFF) | (0x7 << 16) | 0x80000000);
}

constexpr auto _SUCCEEDED(HRESULT hr) -> bool { return SUCCEEDED(hr); }

constexpr auto _FAILED(HRESULT hr) -> bool { return FAILED(hr); }

}  // namespace Vendor::Windows
