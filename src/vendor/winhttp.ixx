module;

#include <windows.h>
#include <winhttp.h>

export module Vendor.WinHttp;

namespace Vendor::WinHttp {

// 导出 WinHTTP 相关类型
export using HINTERNET = ::HINTERNET;
export using URL_COMPONENTS = ::URL_COMPONENTS;
export using DWORD = ::DWORD;
export using INTERNET_PORT = ::INTERNET_PORT;
export using BOOL = ::BOOL;
export using DWORD_PTR = ::DWORD_PTR;

// 导出 WinHTTP 函数
export auto WinHttpOpen(const wchar_t* pszAgentW, DWORD dwAccessType, const wchar_t* pszProxyW,
                        const wchar_t* pszProxyBypassW, DWORD dwFlags) -> HINTERNET {
  return ::WinHttpOpen(pszAgentW, dwAccessType, pszProxyW, pszProxyBypassW, dwFlags);
}

export auto WinHttpCrackUrl(const wchar_t* pwszUrl, DWORD dwUrlLength, DWORD dwFlags,
                            URL_COMPONENTS* lpUrlComponents) -> BOOL {
  return ::WinHttpCrackUrl(pwszUrl, dwUrlLength, dwFlags, lpUrlComponents);
}

export auto WinHttpConnect(HINTERNET hSession, const wchar_t* pswzServerName,
                           INTERNET_PORT nServerPort, DWORD dwReserved) -> HINTERNET {
  return ::WinHttpConnect(hSession, pswzServerName, nServerPort, dwReserved);
}

export auto WinHttpOpenRequest(HINTERNET hConnect, const wchar_t* pwszVerb,
                               const wchar_t* pwszObjectName, const wchar_t* pwszVersion,
                               const wchar_t* pwszReferrer, const wchar_t** ppwszAcceptTypes,
                               DWORD dwFlags) -> HINTERNET {
  return ::WinHttpOpenRequest(hConnect, pwszVerb, pwszObjectName, pwszVersion, pwszReferrer,
                              ppwszAcceptTypes, dwFlags);
}

export auto WinHttpSendRequest(HINTERNET hRequest, const wchar_t* lpszHeaders,
                               DWORD dwHeadersLength, void* lpOptional, DWORD dwOptionalLength,
                               DWORD dwTotalLength, DWORD_PTR dwContext) -> BOOL {
  return ::WinHttpSendRequest(hRequest, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength,
                              dwTotalLength, dwContext);
}

export auto WinHttpReceiveResponse(HINTERNET hRequest, void* lpReserved) -> BOOL {
  return ::WinHttpReceiveResponse(hRequest, lpReserved);
}

export auto WinHttpQueryHeaders(HINTERNET hRequest, DWORD dwInfoLevel, const wchar_t* pwszName,
                                void* lpBuffer, DWORD* lpdwBufferLength, DWORD* lpdwIndex) -> BOOL {
  return ::WinHttpQueryHeaders(hRequest, dwInfoLevel, pwszName, lpBuffer, lpdwBufferLength,
                               lpdwIndex);
}

export auto WinHttpQueryDataAvailable(HINTERNET hRequest, DWORD* lpdwNumberOfBytesAvailable)
    -> BOOL {
  return ::WinHttpQueryDataAvailable(hRequest, lpdwNumberOfBytesAvailable);
}

export auto WinHttpReadData(HINTERNET hRequest, void* lpBuffer, DWORD dwNumberOfBytesToRead,
                            DWORD* lpdwNumberOfBytesRead) -> BOOL {
  return ::WinHttpReadData(hRequest, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);
}

export auto WinHttpCloseHandle(HINTERNET hInternet) -> BOOL {
  return ::WinHttpCloseHandle(hInternet);
}

// 导出常量 (使用 k 前缀风格)
// Access types
export constexpr auto kWINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY = WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY;

// Proxy constants
export constexpr const wchar_t* kWINHTTP_NO_PROXY_NAME = WINHTTP_NO_PROXY_NAME;
export constexpr const wchar_t* kWINHTTP_NO_PROXY_BYPASS = WINHTTP_NO_PROXY_BYPASS;

// Request constants
export constexpr const wchar_t* kWINHTTP_NO_REFERER = WINHTTP_NO_REFERER;
export constexpr const wchar_t** kWINHTTP_DEFAULT_ACCEPT_TYPES = WINHTTP_DEFAULT_ACCEPT_TYPES;
export constexpr auto kWINHTTP_FLAG_SECURE = WINHTTP_FLAG_SECURE;

// Header constants
export constexpr const wchar_t* kWINHTTP_NO_ADDITIONAL_HEADERS = WINHTTP_NO_ADDITIONAL_HEADERS;
export constexpr void* kWINHTTP_NO_REQUEST_DATA = WINHTTP_NO_REQUEST_DATA;

// Query constants
export constexpr auto kWINHTTP_QUERY_STATUS_CODE = WINHTTP_QUERY_STATUS_CODE;
export constexpr auto kWINHTTP_QUERY_FLAG_NUMBER = WINHTTP_QUERY_FLAG_NUMBER;
export constexpr const wchar_t* kWINHTTP_HEADER_NAME_BY_INDEX = WINHTTP_HEADER_NAME_BY_INDEX;
// Note: WINHTTP_NO_HEADER_INDEX is NULL, so we just use nullptr directly

// URL scheme constants
export constexpr auto kINTERNET_SCHEME_HTTPS = INTERNET_SCHEME_HTTPS;

}  // namespace Vendor::WinHttp