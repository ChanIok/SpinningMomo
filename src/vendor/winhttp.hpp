#pragma once

#include <windows.h>
#include <winhttp.h>

namespace Vendor::WinHttp {

// 导出 WinHTTP 相关类型
using HINTERNET = ::HINTERNET;
using URL_COMPONENTS = ::URL_COMPONENTS;
using DWORD = ::DWORD;
using INTERNET_PORT = ::INTERNET_PORT;
using BOOL = ::BOOL;
using DWORD_PTR = ::DWORD_PTR;
using LPVOID = ::LPVOID;
using WINHTTP_STATUS_CALLBACK = ::WINHTTP_STATUS_CALLBACK;
using WINHTTP_ASYNC_RESULT = ::WINHTTP_ASYNC_RESULT;

// 导出 WinHTTP 函数
inline auto WinHttpOpen(const wchar_t* pszAgentW, DWORD dwAccessType, const wchar_t* pszProxyW,
                        const wchar_t* pszProxyBypassW, DWORD dwFlags) -> HINTERNET {
  return ::WinHttpOpen(pszAgentW, dwAccessType, pszProxyW, pszProxyBypassW, dwFlags);
}

inline auto WinHttpCrackUrl(const wchar_t* pwszUrl, DWORD dwUrlLength, DWORD dwFlags,
                            URL_COMPONENTS* lpUrlComponents) -> BOOL {
  return ::WinHttpCrackUrl(pwszUrl, dwUrlLength, dwFlags, lpUrlComponents);
}

inline auto WinHttpConnect(HINTERNET hSession, const wchar_t* pswzServerName,
                           INTERNET_PORT nServerPort, DWORD dwReserved) -> HINTERNET {
  return ::WinHttpConnect(hSession, pswzServerName, nServerPort, dwReserved);
}

inline auto WinHttpOpenRequest(HINTERNET hConnect, const wchar_t* pwszVerb,
                               const wchar_t* pwszObjectName, const wchar_t* pwszVersion,
                               const wchar_t* pwszReferrer, const wchar_t** ppwszAcceptTypes,
                               DWORD dwFlags) -> HINTERNET {
  return ::WinHttpOpenRequest(hConnect, pwszVerb, pwszObjectName, pwszVersion, pwszReferrer,
                              ppwszAcceptTypes, dwFlags);
}

inline auto WinHttpSendRequest(HINTERNET hRequest, const wchar_t* lpszHeaders,
                               DWORD dwHeadersLength, void* lpOptional, DWORD dwOptionalLength,
                               DWORD dwTotalLength, DWORD_PTR dwContext) -> BOOL {
  return ::WinHttpSendRequest(hRequest, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength,
                              dwTotalLength, dwContext);
}

inline auto WinHttpReceiveResponse(HINTERNET hRequest, void* lpReserved) -> BOOL {
  return ::WinHttpReceiveResponse(hRequest, lpReserved);
}

inline auto WinHttpQueryHeaders(HINTERNET hRequest, DWORD dwInfoLevel, const wchar_t* pwszName,
                                void* lpBuffer, DWORD* lpdwBufferLength, DWORD* lpdwIndex) -> BOOL {
  return ::WinHttpQueryHeaders(hRequest, dwInfoLevel, pwszName, lpBuffer, lpdwBufferLength,
                               lpdwIndex);
}

inline auto WinHttpQueryDataAvailable(HINTERNET hRequest, DWORD* lpdwNumberOfBytesAvailable)
    -> BOOL {
  return ::WinHttpQueryDataAvailable(hRequest, lpdwNumberOfBytesAvailable);
}

inline auto WinHttpReadData(HINTERNET hRequest, void* lpBuffer, DWORD dwNumberOfBytesToRead,
                            DWORD* lpdwNumberOfBytesRead) -> BOOL {
  return ::WinHttpReadData(hRequest, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);
}

inline auto WinHttpCloseHandle(HINTERNET hInternet) -> BOOL {
  return ::WinHttpCloseHandle(hInternet);
}

inline auto WinHttpSetStatusCallback(HINTERNET hInternet,
                                     WINHTTP_STATUS_CALLBACK lpfnInternetCallback,
                                     DWORD dwNotificationFlags, DWORD_PTR dwReserved)
    -> WINHTTP_STATUS_CALLBACK {
  return ::WinHttpSetStatusCallback(hInternet, lpfnInternetCallback, dwNotificationFlags,
                                    dwReserved);
}

inline auto WinHttpSetOption(HINTERNET hInternet, DWORD dwOption, LPVOID lpBuffer,
                             DWORD dwBufferLength) -> BOOL {
  return ::WinHttpSetOption(hInternet, dwOption, lpBuffer, dwBufferLength);
}

inline auto WinHttpSetTimeouts(HINTERNET hInternet, int nResolveTimeout, int nConnectTimeout,
                               int nSendTimeout, int nReceiveTimeout) -> BOOL {
  return ::WinHttpSetTimeouts(hInternet, nResolveTimeout, nConnectTimeout, nSendTimeout,
                              nReceiveTimeout);
}

struct UniqueHInternet {
  HINTERNET handle = nullptr;

  UniqueHInternet() = default;
  explicit UniqueHInternet(HINTERNET h) : handle(h) {}
  ~UniqueHInternet() {
    if (handle) ::WinHttpCloseHandle(handle);
  }
  UniqueHInternet(const UniqueHInternet&) = delete;
  UniqueHInternet& operator=(const UniqueHInternet&) = delete;
  UniqueHInternet(UniqueHInternet&& o) noexcept : handle(std::exchange(o.handle, nullptr)) {}
  UniqueHInternet& operator=(UniqueHInternet&& o) noexcept {
    if (this != &o) {
      if (handle) ::WinHttpCloseHandle(handle);
      handle = std::exchange(o.handle, nullptr);
    }
    return *this;
  }
  explicit operator bool() const { return handle != nullptr; }
  inline auto get() const -> HINTERNET { return handle; }
};

// 导出常量 (使用 k 前缀风格)
// Access types
constexpr auto kWINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY = WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY;

// Proxy constants
constexpr const wchar_t* kWINHTTP_NO_PROXY_NAME = WINHTTP_NO_PROXY_NAME;
constexpr const wchar_t* kWINHTTP_NO_PROXY_BYPASS = WINHTTP_NO_PROXY_BYPASS;
constexpr auto kWINHTTP_FLAG_ASYNC = WINHTTP_FLAG_ASYNC;

// Request constants
constexpr const wchar_t* kWINHTTP_NO_REFERER = WINHTTP_NO_REFERER;
constexpr const wchar_t** kWINHTTP_DEFAULT_ACCEPT_TYPES = WINHTTP_DEFAULT_ACCEPT_TYPES;
constexpr auto kWINHTTP_FLAG_SECURE = WINHTTP_FLAG_SECURE;

// Header constants
constexpr const wchar_t* kWINHTTP_NO_ADDITIONAL_HEADERS = WINHTTP_NO_ADDITIONAL_HEADERS;
constexpr void* kWINHTTP_NO_REQUEST_DATA = WINHTTP_NO_REQUEST_DATA;

// Query constants
constexpr auto kWINHTTP_QUERY_STATUS_CODE = WINHTTP_QUERY_STATUS_CODE;
constexpr auto kWINHTTP_QUERY_FLAG_NUMBER = WINHTTP_QUERY_FLAG_NUMBER;
constexpr const wchar_t* kWINHTTP_HEADER_NAME_BY_INDEX = WINHTTP_HEADER_NAME_BY_INDEX;
constexpr auto kWINHTTP_QUERY_RAW_HEADERS_CRLF = WINHTTP_QUERY_RAW_HEADERS_CRLF;
// Note: WINHTTP_NO_HEADER_INDEX is NULL, so we just use nullptr directly

// Async callback flags
constexpr auto kWINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE =
    WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE;
constexpr auto kWINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE =
    WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE;
constexpr auto kWINHTTP_CALLBACK_STATUS_DATA_AVAILABLE = WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE;
constexpr auto kWINHTTP_CALLBACK_STATUS_READ_COMPLETE = WINHTTP_CALLBACK_STATUS_READ_COMPLETE;
constexpr auto kWINHTTP_CALLBACK_STATUS_REQUEST_ERROR = WINHTTP_CALLBACK_STATUS_REQUEST_ERROR;
constexpr auto kWINHTTP_CALLBACK_STATUS_HANDLE_CLOSING = WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING;
constexpr auto kWINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS = WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS;
constexpr auto kWINHTTP_CALLBACK_FLAG_HANDLES = WINHTTP_CALLBACK_FLAG_HANDLES;
constexpr auto kWINHTTP_CALLBACK_FLAG_REDIRECT = WINHTTP_CALLBACK_FLAG_REDIRECT;
inline const auto kWINHTTP_INVALID_STATUS_CALLBACK = WINHTTP_INVALID_STATUS_CALLBACK;

// Option constants
constexpr auto kWINHTTP_OPTION_CONTEXT_VALUE = WINHTTP_OPTION_CONTEXT_VALUE;

// Common error constants
constexpr auto kERROR_IO_PENDING = ERROR_IO_PENDING;

// URL scheme constants
constexpr auto kINTERNET_SCHEME_HTTPS = INTERNET_SCHEME_HTTPS;

}  // namespace Vendor::WinHttp
