module;

#include <windows.h>

export module Vendor.Windows;

namespace Vendor::Windows {

// Types
export using BOOL = ::BOOL;
export using DWORD = ::DWORD;
export using UINT = ::UINT;
export using WPARAM = ::WPARAM;
export using LPARAM = ::LPARAM;
export using LRESULT = ::LRESULT;
export using HWND = ::HWND;
export using HINSTANCE = ::HINSTANCE;
export using LPWSTR = ::LPWSTR;
export using LPCWSTR = ::LPCWSTR;
export using POINT = ::POINT;
export using RECT = ::RECT;
export using SIZE = ::SIZE;
export using MSG = ::MSG;
export using OSVERSIONINFOEXW = ::OSVERSIONINFOEXW;
export using LPOSVERSIONINFOW = ::LPOSVERSIONINFOW;
export using WIN32_FILE_ATTRIBUTE_DATA = ::WIN32_FILE_ATTRIBUTE_DATA;
export using FILETIME = ::FILETIME;
export using ULARGE_INTEGER = ::ULARGE_INTEGER;
export using LPVOID = ::LPVOID;
export using GET_FILEEX_INFO_LEVELS = ::GET_FILEEX_INFO_LEVELS;
export using HANDLE = ::HANDLE;
export using HRESULT = ::HRESULT;

// Constants
export constexpr UINT kMB_ICONERROR = MB_ICONERROR;
export constexpr UINT kWM_USER = WM_USER;
export constexpr UINT kWM_QUIT = WM_QUIT;
export constexpr UINT kPM_REMOVE = PM_REMOVE;
export constexpr DWORD kQS_ALLINPUT = QS_ALLINPUT;
export constexpr DWORD kMWMO_INPUTAVAILABLE = MWMO_INPUTAVAILABLE;
export constexpr UINT kMOD_CONTROL = MOD_CONTROL;
export constexpr UINT kMOD_ALT = MOD_ALT;
export constexpr UINT kSWP_NOZORDER = SWP_NOZORDER;
export constexpr UINT kSWP_NOACTIVATE = SWP_NOACTIVATE;
export constexpr GET_FILEEX_INFO_LEVELS kGetFileExInfoStandard = ::GetFileExInfoStandard;
export constexpr DWORD kMAX_PATH = MAX_PATH;
export constexpr DWORD kERROR_CANCELLED = ERROR_CANCELLED;

// System metrics
export auto GetSystemMetrics(int nIndex) -> int { return ::GetSystemMetrics(nIndex); }
export auto GetScreenWidth() -> int { return ::GetSystemMetrics(SM_CXSCREEN); }
export auto GetScreenHeight() -> int { return ::GetSystemMetrics(SM_CYSCREEN); }

// Message box
export auto MessageBoxW(HWND hWnd, const wchar_t* lpText, const wchar_t* lpCaption, UINT uType)
    -> int {
  return ::MessageBoxW(hWnd, lpText, lpCaption, uType);
}
export auto MessageBoxA(HWND hWnd, const char* lpText, const char* lpCaption, UINT uType) -> int {
  return ::MessageBoxA(hWnd, lpText, lpCaption, uType);
}

// Message loop
export auto GetWindowMessage(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
    -> BOOL {
  return ::GetMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
}
export auto PeekMessageW(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax,
                         UINT wRemoveMsg) -> BOOL {
  return ::PeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
}
export auto TranslateWindowMessage(const MSG* lpMsg) -> BOOL { return ::TranslateMessage(lpMsg); }
export auto DispatchWindowMessageW(const MSG* lpMsg) -> LRESULT {
  return ::DispatchMessageW(lpMsg);
}

// Window operations
export auto GetForegroundWindow() -> HWND { return ::GetForegroundWindow(); }
export auto GetWindowRect(HWND hWnd, RECT* lpRect) -> BOOL { return ::GetWindowRect(hWnd, lpRect); }
export auto SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int x, int y, int cx, int cy, UINT uFlags)
    -> BOOL {
  return ::SetWindowPos(hWnd, hWndInsertAfter, x, y, cx, cy, uFlags);
}
export auto InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase) -> BOOL {
  return ::InvalidateRect(hWnd, lpRect, bErase);
}

// Application control
export auto PostQuitMessage(int nExitCode) -> void { ::PostQuitMessage(nExitCode); }

// Synchronization
export auto MsgWaitForMultipleObjectsEx(DWORD nCount, const HANDLE* pHandles, DWORD dwMilliseconds,
                                        DWORD dwWakeMask, DWORD dwFlags) -> DWORD {
  return ::MsgWaitForMultipleObjectsEx(nCount, pHandles, dwMilliseconds, dwWakeMask, dwFlags);
}

// File operations
export auto GetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId,
                                 LPVOID lpFileInformation) -> BOOL {
  return ::GetFileAttributesExW(lpFileName, fInfoLevelId, lpFileInformation);
}

// INI configuration
export auto GetPrivateProfileStringW(LPCWSTR lpAppName, LPCWSTR lpKeyName, LPCWSTR lpDefault,
                                     LPWSTR lpReturnedString, DWORD nSize, LPCWSTR lpFileName)
    -> DWORD {
  return ::GetPrivateProfileStringW(lpAppName, lpKeyName, lpDefault, lpReturnedString, nSize,
                                    lpFileName);
}

// Error handling
export auto GetLastError() -> DWORD { return ::GetLastError(); }

// Handle operations
export auto CloseHandle(HANDLE hObject) -> BOOL { return ::CloseHandle(hObject); }

// HRESULT handling functions (replacing macros)
export constexpr auto _HRESULT_FROM_WIN32(DWORD x) -> HRESULT {
  return static_cast<HRESULT>(x) <= 0
             ? static_cast<HRESULT>(x)
             : static_cast<HRESULT>((x & 0x0000FFFF) | (0x7 << 16) | 0x80000000);
}

export constexpr auto _SUCCEEDED(HRESULT hr) -> bool { return SUCCEEDED(hr); }

export constexpr auto _FAILED(HRESULT hr) -> bool { return FAILED(hr); }

}  // namespace Vendor::Windows