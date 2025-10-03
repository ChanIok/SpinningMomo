module;

#include <wil/com.h>
#include <WebView2.h>  // 必须放最后面

module Core.WebView;

import std;
import Core.State;
import Features.Gallery.State;
import Core.WebView.State;
import Core.WebView.RpcBridge;
import Core.WebView.DragHandler;
import Utils.Logger;
import Utils.String;
import Vendor.BuildConfig;
import Vendor.WIL;
import <Shlwapi.h>;
import <WebView2EnvironmentOptions.h>;
import <windows.h>;
import <wrl.h>;

// 匿名命名空间，用于封装辅助函数
namespace {

// 检查WebView2运行时版本是否支持拖拽区域功能
bool is_drag_region_supported() {
  LPWSTR version_info = nullptr;
  HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version_info);

  if (SUCCEEDED(hr) && version_info) {
    // 将版本字符串转换为数字进行比较
    std::wstring version_str(version_info);
    CoTaskMemFree(version_info);

    // 查找版本号（格式通常是：110.0.1587.41）
    // 我们只关心主版本号
    size_t dot_pos = version_str.find(L'.');
    if (dot_pos != std::wstring::npos) {
      std::wstring major_version_str = version_str.substr(0, dot_pos);
      int major_version = _wtoi(major_version_str.c_str());

      // 版本110及以上支持拖拽区域功能
      return major_version >= 110;
    }
  }

  // 默认假设不支持，使用后备方案
  return false;
}

auto extract_thumbnail_relative_path(const std::wstring& uri, const std::wstring& prefix)
    -> std::optional<std::wstring> {
  if (uri.rfind(prefix, 0) != 0) {
    return std::nullopt;
  }

  std::wstring relative_path = uri.substr(prefix.size());

  if (relative_path.empty()) {
    return std::nullopt;
  }

  if (relative_path.front() == L'/') {
    relative_path.erase(relative_path.begin());
  }

  if (auto query_pos = relative_path.find(L'?'); query_pos != std::wstring::npos) {
    relative_path.erase(query_pos);
  }

  if (auto fragment_pos = relative_path.find(L'#'); fragment_pos != std::wstring::npos) {
    relative_path.erase(fragment_pos);
  }

  if (relative_path.empty()) {
    return std::nullopt;
  }

  return relative_path;
}

auto resolve_thumbnail_path(Core::State::AppState& state, const std::wstring& relative_path)
    -> std::optional<std::filesystem::path> {
  if (!state.gallery || state.gallery->thumbnails_directory.empty()) {
    return std::nullopt;
  }

  std::filesystem::path base_dir = state.gallery->thumbnails_directory;
  std::filesystem::path combined_path = base_dir / std::filesystem::path(relative_path);

  std::error_code ec;
  auto normalized_base = std::filesystem::weakly_canonical(base_dir, ec);
  if (ec) {
    normalized_base = base_dir.lexically_normal();
    ec.clear();
  }

  auto normalized_target = std::filesystem::weakly_canonical(combined_path, ec);
  if (ec) {
    return std::nullopt;
  }

  auto relative = std::filesystem::relative(normalized_target, normalized_base, ec);
  if (ec || relative.native().starts_with(L"..")) {
    return std::nullopt;
  }

  if (!std::filesystem::exists(normalized_target, ec) || ec) {
    return std::nullopt;
  }

  return normalized_target;
}

auto create_thumbnail_response_headers() -> std::wstring {
  return L"Content-Type: image/webp\r\nCache-Control: public, max-age=86400\r\n";
}

auto handle_thumbnail_web_resource_request(Core::State::AppState& state,
                                           const std::wstring& thumbnail_prefix,
                                           ICoreWebView2Environment* environment,
                                           ICoreWebView2WebResourceRequestedEventArgs* args)
    -> HRESULT {
  Logger().info("Handling thumbnail web resource request: {}",
                Utils::String::ToUtf8(thumbnail_prefix));
  if (!environment) return S_OK;

  wil::com_ptr<ICoreWebView2WebResourceRequest> request;
  if (FAILED(args->get_Request(request.put())) || !request) return S_OK;

  wil::unique_cotaskmem_string uri_raw;
  if (FAILED(request->get_Uri(&uri_raw)) || !uri_raw) return S_OK;

  std::wstring uri(uri_raw.get());

  auto relative_path = extract_thumbnail_relative_path(uri, thumbnail_prefix);
  if (!relative_path) return S_OK;

  auto thumbnail_path = resolve_thumbnail_path(state, *relative_path);
  if (!thumbnail_path) {
    Logger().warn("Thumbnail not found or inaccessible: {}", Utils::String::ToUtf8(*relative_path));

    wil::com_ptr<ICoreWebView2WebResourceResponse> not_found_response;
    if (SUCCEEDED(environment->CreateWebResourceResponse(nullptr, 404, L"Not Found", nullptr,
                                                         not_found_response.put()))) {
      args->put_Response(not_found_response.get());
    }
    return S_OK;
  }

  wil::com_ptr<IStream> stream;
  HRESULT hr = SHCreateStreamOnFileEx(thumbnail_path->c_str(), STGM_READ | STGM_SHARE_DENY_WRITE,
                                      FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, stream.put());

  if (FAILED(hr) || !stream) {
    Logger().error("Failed to open thumbnail file: {} (hr={})",
                   Utils::String::ToUtf8(thumbnail_path->wstring()), hr);
    return S_OK;
  }

  wil::com_ptr<ICoreWebView2WebResourceResponse> response;
  auto headers = create_thumbnail_response_headers();

  if (FAILED(environment->CreateWebResourceResponse(stream.get(), 200, L"OK", headers.c_str(),
                                                    response.put())) ||
      !response) {
    Logger().error("Failed to create WebView2 response for thumbnail {}",
                   Utils::String::ToUtf8(thumbnail_path->wstring()));
    return S_OK;
  }

  args->put_Response(response.get());
  Logger().debug("Served thumbnail via WebResourceRequested: {}",
                 Utils::String::ToUtf8(thumbnail_path->wstring()));
  return S_OK;
}

}  // namespace

// 匿名命名空间，用于封装 COM 回调处理器
namespace {

// 1. 导航事件处理器
class NavigationStartingEventHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ICoreWebView2NavigationStartingEventHandler> {
 private:
  Core::State::AppState* m_state;

 public:
  NavigationStartingEventHandler(Core::State::AppState* state) : m_state(state) {}

  HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender,
                                   ICoreWebView2NavigationStartingEventArgs* args) {
    if (m_state) {
      Logger().debug("WebView navigation starting");
    }
    return S_OK;
  }
};

class NavigationCompletedEventHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ICoreWebView2NavigationCompletedEventHandler> {
 private:
  Core::State::AppState* m_state;

 public:
  NavigationCompletedEventHandler(Core::State::AppState* state) : m_state(state) {}

  HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender,
                                   ICoreWebView2NavigationCompletedEventArgs* args) {
    if (!m_state) return S_OK;

    BOOL success;
    args->get_IsSuccess(&success);

    if (success) {
      Logger().info("WebView navigation completed successfully");
    } else {
      COREWEBVIEW2_WEB_ERROR_STATUS error;
      args->get_WebErrorStatus(&error);
      Logger().error("WebView navigation failed with error: {}", static_cast<int>(error));
    }

    return S_OK;
  }
};

// 2. 控制器创建处理器
class ControllerCompletedHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> {
 private:
  Core::State::AppState* m_state;

  // 辅助方法：设置导航事件
  HRESULT setup_navigation_events(ICoreWebView2* webview,
                                  Core::WebView::State::CoreResources& resources) {
    auto navigation_starting_handler =
        Microsoft::WRL::Make<NavigationStartingEventHandler>(m_state);
    HRESULT hr = webview->add_NavigationStarting(navigation_starting_handler.Get(),
                                                 &resources.navigation_starting_token);
    if (FAILED(hr)) {
      Logger().error("Failed to register NavigationStarting event: {}", hr);
      return hr;
    }

    auto navigation_completed_handler =
        Microsoft::WRL::Make<NavigationCompletedEventHandler>(m_state);
    hr = webview->add_NavigationCompleted(navigation_completed_handler.Get(),
                                          &resources.navigation_completed_token);
    if (FAILED(hr)) {
      Logger().error("Failed to register NavigationCompleted event: {}", hr);
      return hr;
    }

    Logger().debug("Navigation events registered successfully");
    return S_OK;
  }

  // 辅助方法：设置消息处理器
  HRESULT setup_message_handler(ICoreWebView2* webview,
                                Core::WebView::State::CoreResources& resources) {
    auto message_handler = Core::WebView::RpcBridge::create_message_handler(*m_state);

    auto webview_message_handler =
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [message_handler](ICoreWebView2* sender,
                              ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
              try {
                LPWSTR message_raw;
                HRESULT hr = args->get_WebMessageAsJson(&message_raw);

                if (SUCCEEDED(hr) && message_raw) {
                  std::string message = Utils::String::ToUtf8(message_raw);
                  message_handler(message);
                  CoTaskMemFree(message_raw);
                }

                return S_OK;
              } catch (const std::exception& e) {
                Logger().error("Error in WebView message handler: {}", e.what());
                return E_FAIL;
              }
            });

    HRESULT hr = webview->add_WebMessageReceived(webview_message_handler.Get(),
                                                 &resources.web_message_received_token);
    if (FAILED(hr)) {
      Logger().error("Failed to register WebMessageReceived handler: {}", hr);
      return hr;
    }

    Logger().debug("Message handler registered successfully");
    return S_OK;
  }

  // 辅助方法：设置虚拟主机映射
  HRESULT setup_virtual_host_mapping(ICoreWebView2* webview,
                                     Core::WebView::State::WebViewConfig& config) {
    auto dist_path = std::filesystem::absolute(config.frontend_dist_path).wstring();

    wil::com_ptr<ICoreWebView2_3> webview3;
    HRESULT hr = webview->QueryInterface(IID_PPV_ARGS(&webview3));
    if (FAILED(hr)) {
      Logger().error("Failed to query ICoreWebView2_3 interface: {}", hr);
      return hr;
    }

    if (!webview3) {
      Logger().error("ICoreWebView2_3 interface not available");
      return E_NOINTERFACE;
    }

    hr = webview3->SetVirtualHostNameToFolderMapping(
        config.virtual_host_name.c_str(), dist_path.c_str(),
        COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
    if (FAILED(hr)) {
      Logger().error("Failed to set virtual host mapping: {}", hr);
      return hr;
    }

    Logger().info("Virtual host mapping established: {} -> {}",
                  Utils::String::ToUtf8(config.virtual_host_name),
                  Utils::String::ToUtf8(dist_path));
    return S_OK;
  }

  // 辅助方法：设置资源拦截
  HRESULT setup_resource_interception(ICoreWebView2* webview, ICoreWebView2Environment* environment,
                                      Core::WebView::State::CoreResources& resources,
                                      Core::WebView::State::WebViewConfig& config) {
    auto webview2 = wil::com_ptr<ICoreWebView2>(webview).try_query<ICoreWebView2_2>();
    if (!webview2) {
      Logger().warn("ICoreWebView2_2 interface not available, thumbnail interception disabled");
      return S_OK;
    }

    // 根据 debug 模式选择拦截路径
    std::wstring filter;
    std::wstring thumbnail_prefix;
    if (Vendor::BuildConfig::is_debug_build()) {
      // debug 模式：拦截开发服务器的静态资源路径
      filter = config.dev_server_url + L"/static/thumbnails/*";
      thumbnail_prefix = config.dev_server_url + L"/static/thumbnails/";
      Logger().info("Debug mode: Intercepting thumbnails from {}", Utils::String::ToUtf8(filter));
    } else {
      // release 模式：拦截静态资源路径的所有请求
      filter = L"https://" + config.static_host_name + L"/*";
      thumbnail_prefix = L"https://" + config.static_host_name + L"/static/thumbnails/";
      Logger().info("Release mode: Intercepting all requests from {}",
                    Utils::String::ToUtf8(filter));
    }

    HRESULT hr = webview2->AddWebResourceRequestedFilter(filter.c_str(),
                                                         COREWEBVIEW2_WEB_RESOURCE_CONTEXT_IMAGE);
    if (FAILED(hr)) {
      Logger().warn("Failed to add WebResourceRequested filter: {}", hr);
      return hr;
    }

    auto app_state_ptr = m_state;
    auto web_resource_requested_handler =
        Microsoft::WRL::Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [app_state_ptr, thumbnail_prefix, environment](
                ICoreWebView2* sender,
                ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
              return handle_thumbnail_web_resource_request(*app_state_ptr, thumbnail_prefix,
                                                           environment, args);
            });

    EventRegistrationToken token;
    hr = webview->add_WebResourceRequested(web_resource_requested_handler.Get(), &token);
    if (FAILED(hr)) {
      Logger().warn("Failed to register WebResourceRequested handler: {}", hr);
      return hr;
    }

    resources.webresource_requested_tokens.push_back(token);
    Logger().info("Thumbnail WebResourceRequested handler registered");
    return S_OK;
  }

  // 辅助方法：选择初始URL
  void select_initial_url(Core::WebView::State::WebViewConfig& config) {
    if (Vendor::BuildConfig::is_debug_build()) {
      // 开发环境：连接Vite dev server (热重载)
      config.initial_url = config.dev_server_url;
      Logger().info("Debug mode: Using Vite dev server at {}",
                    Utils::String::ToUtf8(config.dev_server_url));
    } else {
      // 生产环境：使用构建产物
      config.initial_url = L"https://" + config.virtual_host_name + L"/index.html";
      Logger().info("Release mode: Using built frontend from resources/web");
    }
  }

  // 辅助方法：设置拖动处理
  HRESULT setup_drag_handling() {
    bool drag_supported = is_drag_region_supported();
    if (!drag_supported) {
      if (auto result = Core::WebView::DragHandler::register_drag_handler(*m_state); !result) {
        Logger().warn("Failed to register drag handler: {}", result.error());
        return E_FAIL;
      }
      Logger().info("Drag handler registered as fallback for older WebView2 versions");
    } else {
      Logger().info("Using native drag region support, COM-based drag handler not needed");
    }
    return S_OK;
  }

  // 辅助方法：初始化导航
  HRESULT initialize_navigation(ICoreWebView2* webview, const std::wstring& initial_url) {
    HRESULT hr = webview->Navigate(initial_url.c_str());
    if (FAILED(hr)) {
      Logger().error("Failed to navigate to initial URL: {}", hr);
      return hr;
    }

    Logger().info("WebView2 ready, navigating to: {}", Utils::String::ToUtf8(initial_url));
    return S_OK;
  }

 public:
  ControllerCompletedHandler(Core::State::AppState* state) : m_state(state) {}

  HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller* controller) {
    if (!m_state) return E_FAIL;
    auto& webview_state = *m_state->webview;

    if (FAILED(result)) {
      Logger().error("Failed to create WebView2 controller: {}", result);
      return result;
    }

    // 1. 保存控制器并获取 WebView 核心接口
    webview_state.resources.controller = controller;
    HRESULT hr = controller->get_CoreWebView2(webview_state.resources.webview.put());
    if (FAILED(hr)) {
      Logger().error("Failed to get CoreWebView2: {}", hr);
      return hr;
    }

    auto* webview = webview_state.resources.webview.get();
    auto* environment = webview_state.resources.environment.get();

    // 2. 设置窗口边界
    RECT bounds = {0, 0, webview_state.window.width, webview_state.window.height};
    controller->put_Bounds(bounds);

    // 3. 设置导航事件
    hr = setup_navigation_events(webview, webview_state.resources);
    if (FAILED(hr)) return hr;

    // 4. 设置消息处理器
    hr = setup_message_handler(webview, webview_state.resources);
    if (FAILED(hr)) return hr;

    // 5. 设置虚拟主机映射
    hr = setup_virtual_host_mapping(webview, webview_state.config);
    if (FAILED(hr)) return hr;

    // 6. 设置资源拦截（缩略图）
    hr = setup_resource_interception(webview, environment, webview_state.resources,
                                     webview_state.config);

    if (FAILED(hr)) {
      // 资源拦截失败不是致命错误，继续执行
      Logger().warn("Resource interception setup failed, continuing without thumbnail support");
    }

    // 7. 选择初始 URL
    select_initial_url(webview_state.config);

    // 8. 设置拖动处理
    hr = setup_drag_handling();
    if (FAILED(hr)) {
      // 拖动处理失败不是致命错误
      Logger().warn("Drag handling setup failed, continuing without drag support");
    }

    // 9. 初始化导航
    hr = initialize_navigation(webview, webview_state.config.initial_url);
    if (FAILED(hr)) return hr;

    // 10. 标记就绪并初始化 RPC 桥接
    webview_state.is_ready = true;
    Core::WebView::RpcBridge::initialize_rpc_bridge(*m_state);

    return S_OK;
  }
};

// 3. 环境创建处理器
class EnvironmentCompletedHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler> {
 private:
  Core::State::AppState* m_state;
  HWND m_webview_hwnd;

 public:
  EnvironmentCompletedHandler(Core::State::AppState* state, HWND webview_hwnd)
      : m_state(state), m_webview_hwnd(webview_hwnd) {}

  HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment* env) {
    if (!m_state) return E_FAIL;
    auto& webview_state = *m_state->webview;

    if (FAILED(result)) {
      Logger().error("Failed to create WebView2 environment: {}", result);
      return result;
    }

    webview_state.resources.environment = env;

    // 现在，使用其自己的处理器创建控制器
    auto controller_handler = Microsoft::WRL::Make<ControllerCompletedHandler>(m_state);
    return env->CreateCoreWebView2Controller(m_webview_hwnd, controller_handler.Get());
  }
};

}  // namespace

namespace Core::WebView {

auto initialize(Core::State::AppState& state, HWND webview_hwnd)
    -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (webview_state.is_initialized) {
    return std::unexpected("WebView already initialized");
  }

  try {
    // 使用 wil RAII 初始化 COM，static 确保整个应用生命周期内有效
    static auto coinit = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);

    // 创建环境选项对象并添加浏览器参数
    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();

    if (options) {
      // 添加启用拖拽区域的参数（在旧版本上会被忽略，不会造成错误）
      Vendor::WIL::throw_if_failed(options->put_AdditionalBrowserArguments(
          L"--enable-features=msWebView2EnableDraggableRegions"));
    }

    // 使用有状态处理器创建环境
    auto environment_handler =
        Microsoft::WRL::Make<EnvironmentCompletedHandler>(&state, webview_hwnd);

    HRESULT hr;
    if (options) {
      hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, options.Get(),
                                                    environment_handler.Get());
    } else {
      Logger().warn("Failed to create WebView2 environment options, trying without options");
      hr = CreateCoreWebView2Environment(environment_handler.Get());
    }

    Vendor::WIL::throw_if_failed(hr);

    webview_state.is_initialized = true;
    Logger().info("WebView2 initialization started with draggable regions support flag");
    return {};

  } catch (const wil::ResultException& e) {
    auto error_msg = std::format("Failed to initialize WebView2: {} (HRESULT: 0x{:08X})", e.what(),
                                 static_cast<unsigned>(e.GetErrorCode()));
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  } catch (const std::exception& e) {
    auto error_msg = std::format("Unexpected error during WebView2 initialization: {}", e.what());
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }
}

auto resize_webview(Core::State::AppState& state, int width, int height) -> void {
  auto& webview_state = *state.webview;

  if (webview_state.resources.controller) {
    webview_state.window.width = width;
    webview_state.window.height = height;

    RECT bounds = {0, 0, width, height};

    webview_state.resources.controller.get()->put_Bounds(bounds);
    Logger().debug("WebView resized to {}x{}", width, height);
  }
}

auto navigate_to_url(Core::State::AppState& state, const std::wstring& url)
    -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (!webview_state.is_ready) {
    return std::unexpected("WebView not ready");
  }

  auto hr = webview_state.resources.webview.get()->Navigate(url.c_str());
  if (FAILED(hr)) {
    return std::unexpected("Failed to navigate to URL");
  }

  webview_state.resources.current_url = url;
  Logger().info("Navigating to: {}", Utils::String::ToUtf8(url));

  return {};
}

auto shutdown(Core::State::AppState& state) -> void {
  auto& webview_state = *state.webview;

  if (webview_state.resources.controller) {
    webview_state.resources.controller.get()->Close();
  }

  webview_state.resources.webview.reset();
  webview_state.resources.controller.reset();
  webview_state.resources.environment.reset();

  webview_state.is_initialized = false;
  webview_state.is_ready = false;
  webview_state.window.is_visible = false;

  Logger().info("WebView shutdown completed");
}

// 消息功能保持不变
auto post_message(Core::State::AppState& state, const std::string& message) -> void {
  auto& webview_state = *state.webview;

  if (webview_state.is_ready) {
    std::wstring wmessage = Utils::String::FromUtf8(message);
    webview_state.resources.webview.get()->PostWebMessageAsJson(wmessage.c_str());
    Logger().debug("Posted message to WebView");
  }
}

auto register_message_handler(Core::State::AppState& state, const std::string& message_type,
                              std::function<void(const std::string&)> handler) -> void {
  auto& webview_state = *state.webview;

  std::lock_guard<std::mutex> lock(webview_state.messaging.message_mutex);
  webview_state.messaging.handlers[message_type] = std::move(handler);
  Logger().debug("Registered message handler for type: {}", message_type);
}

auto send_message(Core::State::AppState& state, const std::string& message)
    -> std::expected<std::string, std::string> {
  // 这是一个简化版本，实际应该实现完整的请求-响应机制
  post_message(state, message);
  return std::string("Message sent");
}

}  // namespace Core::WebView