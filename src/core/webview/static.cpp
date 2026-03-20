module;

#include <wil/com.h>

#include <WebView2.h>  // 必须放最后面

module Core.WebView.Static;

import std;
import Core.State;
import Core.WebView.State;
import Core.WebView.Types;
import Utils.File.Mime;
import Utils.Logger;
import Utils.String;
import Vendor.BuildConfig;
import <Shlwapi.h>;
import <windows.h>;
import <wrl.h>;

namespace Core::WebView::Static {

// ---- 自定义静态资源须支持 Range；否则嵌入式 <video> 无法 seek ----
struct ByteRange {
  std::uint64_t start = 0;
  std::uint64_t end = 0;  // inclusive
};

struct RangeHeaderParseResult {
  bool valid = true;
  std::optional<ByteRange> range;
};

auto parse_range_header(std::string_view header_value, std::uint64_t file_size)
    -> RangeHeaderParseResult {
  if (header_value.empty()) {
    return {};
  }

  // 与 HTTP 静态服务器保持一致：只处理单一 range，拒绝 multipart range。
  if (!header_value.starts_with("bytes=") || file_size == 0) {
    return {.valid = false, .range = std::nullopt};
  }

  auto range_spec = header_value.substr(6);
  auto comma_pos = range_spec.find(',');
  if (comma_pos != std::string_view::npos) {
    return {.valid = false, .range = std::nullopt};
  }

  auto dash_pos = range_spec.find('-');
  if (dash_pos == std::string_view::npos) {
    return {.valid = false, .range = std::nullopt};
  }

  auto start_part = range_spec.substr(0, dash_pos);
  auto end_part = range_spec.substr(dash_pos + 1);

  if (start_part.empty()) {
    std::uint64_t suffix_length = 0;
    auto [ptr, ec] =
        std::from_chars(end_part.data(), end_part.data() + end_part.size(), suffix_length);
    if (ec != std::errc{} || ptr != end_part.data() + end_part.size() || suffix_length == 0) {
      return {.valid = false, .range = std::nullopt};
    }

    auto clamped_length = std::min(suffix_length, file_size);
    return {.valid = true,
            .range = ByteRange{.start = file_size - clamped_length, .end = file_size - 1}};
  }

  std::uint64_t start = 0;
  auto [start_ptr, start_ec] =
      std::from_chars(start_part.data(), start_part.data() + start_part.size(), start);
  if (start_ec != std::errc{} || start_ptr != start_part.data() + start_part.size() ||
      start >= file_size) {
    return {.valid = false, .range = std::nullopt};
  }

  if (end_part.empty()) {
    return {.valid = true, .range = ByteRange{.start = start, .end = file_size - 1}};
  }

  std::uint64_t end = 0;
  auto [end_ptr, end_ec] = std::from_chars(end_part.data(), end_part.data() + end_part.size(), end);
  if (end_ec != std::errc{} || end_ptr != end_part.data() + end_part.size() || end < start) {
    return {.valid = false, .range = std::nullopt};
  }

  return {.valid = true, .range = ByteRange{.start = start, .end = std::min(end, file_size - 1)}};
}

auto get_request_header(ICoreWebView2WebResourceRequest* request, const std::wstring& name)
    -> std::optional<std::wstring> {
  if (!request) {
    return std::nullopt;
  }

  wil::com_ptr<ICoreWebView2HttpRequestHeaders> headers;
  if (FAILED(request->get_Headers(headers.put())) || !headers) {
    return std::nullopt;
  }

  wil::unique_cotaskmem_string value_raw;
  if (FAILED(headers->GetHeader(name.c_str(), &value_raw)) || !value_raw) {
    return std::nullopt;
  }

  return std::wstring(value_raw.get());
}

auto create_memory_stream_from_bytes(const std::vector<char>& bytes) -> wil::com_ptr<IStream> {
  wil::com_ptr<IStream> stream;
  if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, stream.put())) || !stream) {
    return nullptr;
  }

  if (!bytes.empty()) {
    ULONG bytes_written = 0;
    if (FAILED(stream->Write(bytes.data(), static_cast<ULONG>(bytes.size()), &bytes_written)) ||
        bytes_written != bytes.size()) {
      return nullptr;
    }
  }

  LARGE_INTEGER seek_origin{};
  stream->Seek(seek_origin, STREAM_SEEK_SET, nullptr);
  return stream;
}

auto read_file_range(const std::filesystem::path& file_path, const ByteRange& range)
    -> std::expected<std::vector<char>, std::string> {
  // WebView2 的自定义响应没有现成的“文件分段流”，partial response 这里走内存流即可。
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    return std::unexpected("Failed to open file for partial WebView response");
  }

  auto length = range.end - range.start + 1;
  std::vector<char> bytes(static_cast<std::size_t>(length));
  file.seekg(static_cast<std::streamoff>(range.start), std::ios::beg);
  file.read(bytes.data(), static_cast<std::streamsize>(length));

  if (!file && !file.eof()) {
    return std::unexpected("Failed to read requested WebView byte range");
  }

  bytes.resize(static_cast<std::size_t>(file.gcount()));
  if (bytes.size() != length) {
    return std::unexpected("WebView byte range read was shorter than expected");
  }

  return bytes;
}

auto build_response_headers(const std::wstring& content_type, std::uint64_t content_length,
                            std::optional<std::uint64_t> source_file_size = std::nullopt,
                            std::optional<ByteRange> range = std::nullopt) -> std::wstring {
  auto headers = std::format(
      L"Content-Type: {}\r\n"
      L"Cache-Control: public, max-age=86400\r\n"
      L"Accept-Ranges: bytes\r\n"
      L"Content-Length: {}\r\n",
      content_type, content_length);

  if (range.has_value() && source_file_size.has_value()) {
    headers += std::format(L"Content-Range: bytes {}-{}/{}\r\n", range->start, range->end,
                           source_file_size.value());
  }

  return headers;
}

auto try_web_resource_resolve(Core::State::AppState& state, std::wstring_view url)
    -> std::optional<Types::WebResourceResolution> {
  if (!state.webview || !state.webview->resources.web_resolvers) {
    return std::nullopt;
  }

  auto& registry = *state.webview->resources.web_resolvers;
  // 无锁读取（RCU 模式）
  auto resolvers = registry.resolvers.load();

  for (const auto& entry : *resolvers) {
    if (url.starts_with(entry.prefix)) {
      auto result = entry.resolver(url);
      if (result.success) {
        return result;
      }
    }
  }

  return std::nullopt;
}

auto handle_custom_web_resource_request(Core::State::AppState& state,
                                        ICoreWebView2Environment* environment,
                                        ICoreWebView2WebResourceRequestedEventArgs* args)
    -> HRESULT {
  if (!environment) {
    return S_OK;
  }

  wil::com_ptr<ICoreWebView2WebResourceRequest> request;
  if (FAILED(args->get_Request(request.put())) || !request) {
    return S_OK;
  }

  wil::unique_cotaskmem_string uri_raw;
  if (FAILED(request->get_Uri(&uri_raw)) || !uri_raw) {
    return S_OK;
  }

  std::wstring uri(uri_raw.get());

  auto custom_result = try_web_resource_resolve(state, uri);
  if (!custom_result) {
    return S_OK;
  }

  const auto& resolution = *custom_result;
  if (!resolution.success) {
    Logger().warn("WebView resource resolution failed: {}", resolution.error_message);

    wil::com_ptr<ICoreWebView2WebResourceResponse> not_found_response;
    if (SUCCEEDED(environment->CreateWebResourceResponse(nullptr, 404, L"Not Found", nullptr,
                                                         not_found_response.put()))) {
      args->put_Response(not_found_response.get());
    }
    return S_OK;
  }

  std::error_code file_ec;
  auto file_size = std::filesystem::file_size(resolution.file_path, file_ec);
  if (file_ec) {
    Logger().error("Failed to query custom resource file size: {} ({})",
                   Utils::String::ToUtf8(resolution.file_path.wstring()), file_ec.message());
    return S_OK;
  }

  // 图库原文件常无显式 content_type，须按扩展名补 MIME，否则播放器可能拒播。
  auto content_type = resolution.content_type.value_or(
      Utils::String::FromUtf8(Utils::File::Mime::get_mime_type(resolution.file_path)));
  auto range_header = get_request_header(request.get(), L"Range");
  auto range_parse = parse_range_header(range_header ? Utils::String::ToUtf8(*range_header) : "",
                                        static_cast<std::uint64_t>(file_size));

  if (!range_parse.valid) {
    auto headers = std::format(L"Accept-Ranges: bytes\r\nContent-Range: bytes */{}\r\n",
                               static_cast<std::uint64_t>(file_size));
    wil::com_ptr<ICoreWebView2WebResourceResponse> invalid_range_response;
    if (SUCCEEDED(environment->CreateWebResourceResponse(nullptr, 416, L"Range Not Satisfiable",
                                                         headers.c_str(),
                                                         invalid_range_response.put()))) {
      args->put_Response(invalid_range_response.get());
    }
    return S_OK;
  }

  wil::com_ptr<IStream> stream;
  std::wstring headers;
  int status_code = resolution.status_code.value_or(200);
  const wchar_t* status_text = status_code == 200 ? L"OK" : L"Error";

  // WebView2 CreateWebResourceResponse 无「文件区间流」API；Range 响应只能先读入内存再建 IStream。
  if (range_parse.range.has_value()) {
    auto bytes_result = read_file_range(resolution.file_path, *range_parse.range);
    if (!bytes_result) {
      Logger().error("Failed to read partial custom resource file: {} ({})",
                     Utils::String::ToUtf8(resolution.file_path.wstring()), bytes_result.error());
      return S_OK;
    }

    stream = create_memory_stream_from_bytes(*bytes_result);
    if (!stream) {
      Logger().error("Failed to create memory stream for partial custom resource: {}",
                     Utils::String::ToUtf8(resolution.file_path.wstring()));
      return S_OK;
    }

    headers = build_response_headers(content_type, bytes_result->size(),
                                     static_cast<std::uint64_t>(file_size), range_parse.range);
    status_code = 206;
    status_text = L"Partial Content";
  } else {
    HRESULT hr =
        SHCreateStreamOnFileEx(resolution.file_path.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE,
                               FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, stream.put());
    if (FAILED(hr) || !stream) {
      Logger().error("Failed to open custom resource file: {} (hr={})",
                     Utils::String::ToUtf8(resolution.file_path.wstring()), hr);
      return S_OK;
    }

    headers = build_response_headers(content_type, static_cast<std::uint64_t>(file_size));
  }

  wil::com_ptr<ICoreWebView2WebResourceResponse> response;
  if (FAILED(environment->CreateWebResourceResponse(stream.get(), status_code, status_text,
                                                    headers.c_str(), response.put())) ||
      !response) {
    Logger().error("Failed to create WebView2 response for custom resource {}",
                   Utils::String::ToUtf8(resolution.file_path.wstring()));
    return S_OK;
  }

  args->put_Response(response.get());
  return S_OK;
}

auto register_web_resource_resolver(Core::State::AppState& state, std::wstring prefix,
                                    Types::WebResourceResolver resolver) -> void {
  if (!state.webview || !state.webview->resources.web_resolvers) {
    Logger().error("WebView state not initialized, cannot register resource resolver");
    return;
  }

  auto& registry = *state.webview->resources.web_resolvers;
  std::unique_lock lock(registry.write_mutex);

  // RCU 写入：复制当前 vector，添加新项，然后原子替换
  auto current = registry.resolvers.load();
  auto new_resolvers = std::make_shared<std::vector<Types::WebResolverEntry>>(*current);
  new_resolvers->push_back({std::move(prefix), std::move(resolver)});
  registry.resolvers.store(new_resolvers);

  Logger().debug("Registered WebView resource resolver for: {}", Utils::String::ToUtf8(prefix));
}

auto unregister_web_resource_resolver(Core::State::AppState& state, std::wstring_view prefix)
    -> void {
  if (!state.webview || !state.webview->resources.web_resolvers) {
    return;
  }

  auto& registry = *state.webview->resources.web_resolvers;
  std::unique_lock lock(registry.write_mutex);

  // RCU 写入：复制当前 vector，删除匹配项，然后原子替换
  auto current = registry.resolvers.load();
  auto new_resolvers = std::make_shared<std::vector<Types::WebResolverEntry>>(*current);
  std::erase_if(*new_resolvers, [prefix](const auto& entry) { return entry.prefix == prefix; });
  registry.resolvers.store(new_resolvers);

  Logger().debug("Unregistered WebView resource resolver for: {}",
                 Utils::String::ToUtf8(std::wstring(prefix)));
}

auto setup_resource_interception(Core::State::AppState& state, ICoreWebView2* webview,
                                 ICoreWebView2Environment* environment,
                                 Core::WebView::State::CoreResources& resources,
                                 Core::WebView::State::WebViewConfig& config) -> HRESULT {
  auto webview2 = wil::com_ptr<ICoreWebView2>(webview).try_query<ICoreWebView2_2>();
  if (!webview2) {
    Logger().warn("ICoreWebView2_2 interface not available, custom resource interception disabled");
    return S_OK;
  }

  std::wstring filter;
  if (Vendor::BuildConfig::is_debug_build()) {
    filter = config.dev_server_url + L"/static/*";
    Logger().info("Debug mode: Intercepting static resources from {}",
                  Utils::String::ToUtf8(filter));
  } else {
    filter = L"https://" + config.static_host_name + L"/*";
    Logger().info("Release mode: Intercepting static resources from {}",
                  Utils::String::ToUtf8(filter));
  }

  HRESULT hr = webview2->AddWebResourceRequestedFilter(filter.c_str(),
                                                       COREWEBVIEW2_WEB_RESOURCE_CONTEXT_IMAGE);
  if (FAILED(hr)) {
    Logger().warn("Failed to add image WebResourceRequested filter: {}", hr);
    return hr;
  }

  // <video src> 等请求常落在 MEDIA/OTHER，仅挂 IMAGE 时无法拦截图库原片 URL。
  hr = webview2->AddWebResourceRequestedFilter(filter.c_str(),
                                               COREWEBVIEW2_WEB_RESOURCE_CONTEXT_MEDIA);
  if (FAILED(hr)) {
    Logger().warn("Failed to add media WebResourceRequested filter: {}", hr);
    return hr;
  }

  // OTHER：兜底部分导航/子资源上下文，避免漏拦。
  hr = webview2->AddWebResourceRequestedFilter(filter.c_str(),
                                               COREWEBVIEW2_WEB_RESOURCE_CONTEXT_OTHER);
  if (FAILED(hr)) {
    Logger().warn("Failed to add WebResourceRequested filter: {}", hr);
    return hr;
  }

  auto app_state_ptr = &state;
  auto web_resource_requested_handler =
      Microsoft::WRL::Callback<ICoreWebView2WebResourceRequestedEventHandler>(
          [app_state_ptr, environment](
              ICoreWebView2* sender, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
            return handle_custom_web_resource_request(*app_state_ptr, environment, args);
          });

  EventRegistrationToken token;
  hr = webview->add_WebResourceRequested(web_resource_requested_handler.Get(), &token);
  if (FAILED(hr)) {
    Logger().warn("Failed to register WebResourceRequested handler: {}", hr);
    return hr;
  }

  resources.webresource_requested_tokens.push_back(token);
  Logger().info("Custom WebResourceRequested handler registered");
  return S_OK;
}

}  // namespace Core::WebView::Static
