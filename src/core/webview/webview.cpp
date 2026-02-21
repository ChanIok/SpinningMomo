module;

#include <wil/com.h>

#include <WebView2.h>  // 必须放最后面

module Core.WebView;

import std;
import Core.State;
import Core.WebView.RpcBridge;
import Core.WebView.State;
import Core.WebView.Static;
import Utils.Logger;
import Utils.String;
import Vendor.BuildConfig;
import Vendor.WIL;
import <d3d11.h>;
import <dcomp.h>;
import <dxgi.h>;
import <windows.h>;
import <windowsx.h>;
import <wrl.h>;

// 匿名命名空间，用于封装辅助函数和 COM 回调处理器
namespace {

struct CompositionHostResources {
  wil::com_ptr<ID3D11Device> d3d_device;
  wil::com_ptr<IDCompositionDevice> dcomp_device;
  wil::com_ptr<IDCompositionTarget> dcomp_target;
  wil::com_ptr<IDCompositionVisual> dcomp_root_visual;
};

std::unordered_map<HWND, CompositionHostResources> g_composition_hosts;

auto create_composition_host(HWND hwnd) -> std::expected<CompositionHostResources, std::string> {
  CompositionHostResources host;

  UINT d3d_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
  d3d_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  wil::com_ptr<ID3D11DeviceContext> d3d_context;
  HRESULT hr =
      D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, d3d_flags, nullptr, 0,
                        D3D11_SDK_VERSION, host.d3d_device.put(), nullptr, d3d_context.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to create D3D11 device for WebView composition: 0x{:08X}",
                    static_cast<unsigned>(hr)));
  }

  wil::com_ptr<IDXGIDevice> dxgi_device;
  hr = host.d3d_device->QueryInterface(IID_PPV_ARGS(&dxgi_device));
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to query IDXGIDevice for WebView composition: 0x{:08X}",
                    static_cast<unsigned>(hr)));
  }

  hr = DCompositionCreateDevice(dxgi_device.get(), IID_PPV_ARGS(&host.dcomp_device));
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to create DComposition device: 0x{:08X}", static_cast<unsigned>(hr)));
  }

  hr = host.dcomp_device->CreateTargetForHwnd(hwnd, TRUE, host.dcomp_target.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to create DComposition target for WebView window: 0x{:08X}",
                    static_cast<unsigned>(hr)));
  }

  hr = host.dcomp_device->CreateVisual(host.dcomp_root_visual.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create DComposition root visual: 0x{:08X}",
                                       static_cast<unsigned>(hr)));
  }

  hr = host.dcomp_target->SetRoot(host.dcomp_root_visual.get());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to set DComposition root visual: 0x{:08X}", static_cast<unsigned>(hr)));
  }

  hr = host.dcomp_device->Commit();
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to commit initial DComposition tree: 0x{:08X}",
                                       static_cast<unsigned>(hr)));
  }

  return host;
}

auto apply_transparent_default_background(ICoreWebView2Controller* controller) -> void {
  wil::com_ptr<ICoreWebView2Controller2> controller2;
  if (FAILED(controller->QueryInterface(IID_PPV_ARGS(&controller2))) || !controller2) {
    Logger().warn("ICoreWebView2Controller2 unavailable, transparent background not applied");
    return;
  }

  COREWEBVIEW2_COLOR transparent{0, 0, 0, 0};
  auto hr = controller2->put_DefaultBackgroundColor(transparent);
  if (FAILED(hr)) {
    Logger().warn("Failed to apply transparent WebView background: {}", hr);
    return;
  }

  Logger().info("WebView2 default background set to transparent");
}

auto to_mouse_virtual_keys(WPARAM wparam) -> COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS {
  COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS keys = COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE;
  if (wparam & MK_LBUTTON)
    keys = static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(
        keys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_LEFT_BUTTON);
  if (wparam & MK_RBUTTON)
    keys = static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(
        keys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_RIGHT_BUTTON);
  if (wparam & MK_MBUTTON)
    keys = static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(
        keys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_MIDDLE_BUTTON);
  if (wparam & MK_XBUTTON1)
    keys = static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(
        keys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON1);
  if (wparam & MK_XBUTTON2)
    keys = static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(
        keys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON2);
  if (wparam & MK_SHIFT)
    keys = static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(
        keys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_SHIFT);
  if (wparam & MK_CONTROL)
    keys = static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(
        keys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_CONTROL);
  return keys;
}

auto to_mouse_event_kind(UINT msg) -> std::optional<COREWEBVIEW2_MOUSE_EVENT_KIND> {
  switch (msg) {
    case WM_MOUSEMOVE:
      return COREWEBVIEW2_MOUSE_EVENT_KIND_MOVE;
    case WM_LBUTTONDOWN:
      return COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOWN;
    case WM_LBUTTONUP:
      return COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_UP;
    case WM_LBUTTONDBLCLK:
      return COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOUBLE_CLICK;
    case WM_RBUTTONDOWN:
      return COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOWN;
    case WM_RBUTTONUP:
      return COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_UP;
    case WM_RBUTTONDBLCLK:
      return COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOUBLE_CLICK;
    case WM_MBUTTONDOWN:
      return COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_DOWN;
    case WM_MBUTTONUP:
      return COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_UP;
    case WM_MBUTTONDBLCLK:
      return COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_DOUBLE_CLICK;
    case WM_XBUTTONDOWN:
      return COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_DOWN;
    case WM_XBUTTONUP:
      return COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_UP;
    case WM_XBUTTONDBLCLK:
      return COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_DOUBLE_CLICK;
    case WM_MOUSEWHEEL:
      return COREWEBVIEW2_MOUSE_EVENT_KIND_WHEEL;
    case WM_MOUSEHWHEEL:
      return COREWEBVIEW2_MOUSE_EVENT_KIND_HORIZONTAL_WHEEL;
    default:
      return std::nullopt;
  }
}

auto to_non_client_hit_test(COREWEBVIEW2_NON_CLIENT_REGION_KIND kind) -> LRESULT {
  switch (kind) {
    case COREWEBVIEW2_NON_CLIENT_REGION_KIND_CAPTION:
      return HTCAPTION;
    case COREWEBVIEW2_NON_CLIENT_REGION_KIND_MINIMIZE:
      return HTMINBUTTON;
    case COREWEBVIEW2_NON_CLIENT_REGION_KIND_MAXIMIZE:
      return HTMAXBUTTON;
    case COREWEBVIEW2_NON_CLIENT_REGION_KIND_CLOSE:
      return HTCLOSE;
    case COREWEBVIEW2_NON_CLIENT_REGION_KIND_CLIENT:
    case COREWEBVIEW2_NON_CLIENT_REGION_KIND_NOWHERE:
    default:
      return HTCLIENT;
  }
}

// ============= 辅助函数 =============

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
          ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler> {
 private:
  Core::State::AppState* m_state;
  HWND m_webview_hwnd;

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

  // 辅助方法：启用 WebView 非客户区支持（标题栏拖拽/窗口按钮命中）
  auto setup_non_client_region_support(ICoreWebView2* webview,
                                       ICoreWebView2CompositionController* composition_controller,
                                       Core::WebView::State::CoreResources& resources) -> void {
    auto hr = composition_controller->QueryInterface(
        IID_PPV_ARGS(resources.composition_controller4.put()));
    if (FAILED(hr) || !resources.composition_controller4) {
      Logger().warn("ICoreWebView2CompositionController4 unavailable, non-client region disabled");
      return;
    }

    wil::com_ptr<ICoreWebView2Settings> settings;
    hr = webview->get_Settings(settings.put());
    if (FAILED(hr) || !settings) {
      Logger().warn("Failed to get WebView settings, non-client region disabled");
      resources.composition_controller4.reset();
      return;
    }

    wil::com_ptr<ICoreWebView2Settings9> settings9;
    hr = settings->QueryInterface(IID_PPV_ARGS(&settings9));
    if (FAILED(hr) || !settings9) {
      Logger().warn("ICoreWebView2Settings9 unavailable, non-client region disabled");
      resources.composition_controller4.reset();
      return;
    }

    hr = settings9->put_IsNonClientRegionSupportEnabled(TRUE);
    if (FAILED(hr)) {
      Logger().warn("Failed to enable non-client region support: {}", hr);
      resources.composition_controller4.reset();
      return;
    }

    Logger().info("WebView non-client region support enabled");
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
  ControllerCompletedHandler(Core::State::AppState* state, HWND webview_hwnd)
      : m_state(state), m_webview_hwnd(webview_hwnd) {}

  HRESULT STDMETHODCALLTYPE Invoke(HRESULT result,
                                   ICoreWebView2CompositionController* composition_controller) {
    if (!m_state) return E_FAIL;
    auto& webview_state = *m_state->webview;

    if (FAILED(result)) {
      Logger().error("Failed to create WebView2 controller: {}", result);
      return result;
    }

    auto host_it = g_composition_hosts.find(m_webview_hwnd);
    if (host_it == g_composition_hosts.end()) {
      Logger().error("Composition host not found for WebView window");
      return E_FAIL;
    }

    // 1. 保存控制器并获取 WebView 核心接口
    webview_state.resources.composition_controller = composition_controller;
    HRESULT hr = composition_controller->QueryInterface(
        IID_PPV_ARGS(webview_state.resources.controller.put()));
    if (FAILED(hr)) {
      Logger().error("Failed to cast composition controller to base controller: {}", hr);
      return hr;
    }

    hr =
        webview_state.resources.controller->get_CoreWebView2(webview_state.resources.webview.put());
    if (FAILED(hr)) {
      Logger().error("Failed to get CoreWebView2: {}", hr);
      return hr;
    }

    hr = composition_controller->put_RootVisualTarget(host_it->second.dcomp_root_visual.get());
    if (FAILED(hr)) {
      Logger().error("Failed to set composition root visual target: {}", hr);
      return hr;
    }

    hr = host_it->second.dcomp_device->Commit();
    if (FAILED(hr)) {
      Logger().error("Failed to commit composition visual tree: {}", hr);
      return hr;
    }

    auto* webview = webview_state.resources.webview.get();
    auto* environment = webview_state.resources.environment.get();

    // 2. 设置窗口边界和透明背景
    RECT bounds = {0, 0, webview_state.window.width, webview_state.window.height};
    webview_state.resources.controller->put_Bounds(bounds);
    apply_transparent_default_background(webview_state.resources.controller.get());

    // 3. 设置导航事件
    hr = setup_navigation_events(webview, webview_state.resources);
    if (FAILED(hr)) return hr;

    // 4. 设置消息处理器
    hr = setup_message_handler(webview, webview_state.resources);
    if (FAILED(hr)) return hr;

    // 5. 设置虚拟主机映射
    hr = setup_virtual_host_mapping(webview, webview_state.config);
    if (FAILED(hr)) {
      if (Vendor::BuildConfig::is_debug_build()) {
        // Debug 模式下使用 dev server，虚拟主机映射失败不是致命错误
        Logger().warn("Virtual host mapping failed in debug mode, continuing with dev server");
      } else {
        return hr;
      }
    }

    // 6. 设置资源拦截（缩略图）
    hr = Core::WebView::Static::setup_resource_interception(
        *m_state, webview, environment, webview_state.resources, webview_state.config);

    if (FAILED(hr)) {
      // 资源拦截失败不是致命错误，继续执行
      Logger().warn("Resource interception setup failed, continuing without thumbnail support");
    }

    // 7. 选择初始 URL
    select_initial_url(webview_state.config);

    // 8. 启用非客户区支持（用于标题栏拖拽和按钮命中）
    setup_non_client_region_support(webview, composition_controller, webview_state.resources);

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

    auto composition_host = create_composition_host(m_webview_hwnd);
    if (!composition_host) {
      Logger().error("Failed to initialize composition host: {}", composition_host.error());
      return E_FAIL;
    }
    g_composition_hosts[m_webview_hwnd] = std::move(composition_host.value());

    wil::com_ptr<ICoreWebView2Environment3> env3;
    HRESULT hr = env->QueryInterface(IID_PPV_ARGS(&env3));
    if (FAILED(hr) || !env3) {
      Logger().error("ICoreWebView2Environment3 is unavailable; composition hosting not supported");
      return FAILED(hr) ? hr : E_NOINTERFACE;
    }

    auto controller_handler =
        Microsoft::WRL::Make<ControllerCompletedHandler>(m_state, m_webview_hwnd);
    hr = env3->CreateCoreWebView2CompositionController(m_webview_hwnd, controller_handler.Get());
    if (FAILED(hr)) {
      Logger().error("Failed to create WebView2 composition controller: {}", hr);
      g_composition_hosts.erase(m_webview_hwnd);
    }
    return hr;
  }
};

}  // namespace

namespace Core::WebView {

// ============= 初始化和生命周期管理 =============

auto initialize(Core::State::AppState& state, HWND webview_hwnd)
    -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (webview_state.is_initialized) {
    return std::unexpected("WebView already initialized");
  }

  try {
    // 使用 wil RAII 初始化 COM，static 确保整个应用生命周期内有效
    static auto coinit = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);

    // 使用有状态处理器创建环境
    auto environment_handler =
        Microsoft::WRL::Make<EnvironmentCompletedHandler>(&state, webview_hwnd);

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
                                                          environment_handler.Get());

    Vendor::WIL::throw_if_failed(hr);

    webview_state.is_initialized = true;
    Logger().info("WebView2 initialization started");
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
  auto webview_hwnd = webview_state.window.webview_hwnd;

  if (webview_state.resources.controller) {
    webview_state.resources.controller.get()->Close();
  }

  webview_state.resources.webview.reset();
  webview_state.resources.composition_controller4.reset();
  webview_state.resources.composition_controller.reset();
  webview_state.resources.controller.reset();
  webview_state.resources.environment.reset();
  if (webview_hwnd) {
    g_composition_hosts.erase(webview_hwnd);
  }

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

auto forward_mouse_message(Core::State::AppState& state, HWND hwnd, UINT msg, WPARAM wparam,
                           LPARAM lparam) -> bool {
  auto& webview_state = *state.webview;
  auto* composition_controller = webview_state.resources.composition_controller.get();
  if (!composition_controller) {
    return false;
  }

  auto event_kind = to_mouse_event_kind(msg);
  if (!event_kind) {
    return false;
  }

  COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS keys = to_mouse_virtual_keys(wparam);

  UINT32 mouse_data = 0;
  if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
    mouse_data = static_cast<UINT32>(GET_WHEEL_DELTA_WPARAM(wparam));
  } else if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONUP || msg == WM_XBUTTONDBLCLK) {
    mouse_data = static_cast<UINT32>(GET_XBUTTON_WPARAM(wparam));
  }

  POINT point;
  point.x = GET_X_LPARAM(lparam);
  point.y = GET_Y_LPARAM(lparam);
  if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
    ScreenToClient(hwnd, &point);
  }

  composition_controller->SendMouseInput(*event_kind, keys, mouse_data, point);
  return true;
}

auto forward_non_client_right_button_message(Core::State::AppState& state, HWND hwnd, UINT msg,
                                             WPARAM wparam, LPARAM lparam) -> bool {
  (void)wparam;

  auto& webview_state = *state.webview;
  auto* composition_controller4 = webview_state.resources.composition_controller4.get();
  if (!composition_controller4) {
    return false;
  }

  COREWEBVIEW2_MOUSE_EVENT_KIND event_kind;
  if (msg == WM_NCRBUTTONDOWN) {
    event_kind = COREWEBVIEW2_MOUSE_EVENT_KIND_NON_CLIENT_RIGHT_BUTTON_DOWN;
  } else if (msg == WM_NCRBUTTONUP) {
    event_kind = COREWEBVIEW2_MOUSE_EVENT_KIND_NON_CLIENT_RIGHT_BUTTON_UP;
  } else {
    return false;
  }

  COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS keys = COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_RIGHT_BUTTON;
  if (GetKeyState(VK_SHIFT) < 0) {
    keys = static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(
        keys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_SHIFT);
  }
  if (GetKeyState(VK_CONTROL) < 0) {
    keys = static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(
        keys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_CONTROL);
  }

  POINT point;
  point.x = GET_X_LPARAM(lparam);
  point.y = GET_Y_LPARAM(lparam);
  ScreenToClient(hwnd, &point);

  composition_controller4->SendMouseInput(event_kind, keys, 0, point);
  return true;
}

auto hit_test_non_client_region(Core::State::AppState& state, HWND hwnd, LPARAM lparam)
    -> std::optional<LRESULT> {
  auto& webview_state = *state.webview;
  auto* composition_controller4 = webview_state.resources.composition_controller4.get();
  if (!composition_controller4) {
    return std::nullopt;
  }

  POINT point;
  point.x = GET_X_LPARAM(lparam);
  point.y = GET_Y_LPARAM(lparam);
  ScreenToClient(hwnd, &point);

  COREWEBVIEW2_NON_CLIENT_REGION_KIND region_kind = COREWEBVIEW2_NON_CLIENT_REGION_KIND_CLIENT;
  auto hr = composition_controller4->GetNonClientRegionAtPoint(point, &region_kind);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  return to_non_client_hit_test(region_kind);
}

auto send_message(Core::State::AppState& state, const std::string& message)
    -> std::expected<std::string, std::string> {
  // 这是一个简化版本，实际应该实现完整的请求-响应机制
  post_message(state, message);
  return std::string("Message sent");
}

}  // namespace Core::WebView
