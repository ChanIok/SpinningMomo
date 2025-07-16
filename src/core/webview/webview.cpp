module;

#include <wil/com.h>
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>  // 必须放最后面

#include <filesystem>

module Core.WebView;

import std;
import Core.WebView.State;
import Core.WebView.RpcBridge;
import Utils.Logger;
import Utils.String;

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
      m_state->webview.is_loading = true;
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

    m_state->webview.is_loading = false;

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

 public:
  ControllerCompletedHandler(Core::State::AppState* state) : m_state(state) {}

  HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller* controller) {
    if (!m_state) return E_FAIL;
    auto& webview_state = m_state->webview;

    if (FAILED(result)) {
      Logger().error("Failed to create WebView2 controller: {}", result);
      return result;
    }

    webview_state.resources.controller = controller;

    // 获取 WebView 核心接口
    auto hr = controller->get_CoreWebView2(webview_state.resources.webview.put());
    if (FAILED(hr)) {
      Logger().error("Failed to get CoreWebView2: {}", hr);
      return hr;
    }

    RECT bounds = {0, 0, webview_state.window.width, webview_state.window.height};
    controller->put_Bounds(bounds);

    // 使用有状态的处理器设置事件回调
    EventRegistrationToken token;

    auto navigation_starting_handler =
        Microsoft::WRL::Make<NavigationStartingEventHandler>(m_state);
    webview_state.resources.webview.get()->add_NavigationStarting(navigation_starting_handler.Get(),
                                                                  &token);

    auto navigation_completed_handler =
        Microsoft::WRL::Make<NavigationCompletedEventHandler>(m_state);
    webview_state.resources.webview.get()->add_NavigationCompleted(
        navigation_completed_handler.Get(), &token);

    // 注册WebView消息处理器
    auto message_handler = Core::WebView::RpcBridge::create_message_handler(*m_state);

    // 创建COM消息处理器
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

    webview_state.resources.webview.get()->add_WebMessageReceived(webview_message_handler.Get(),
                                                                  &token);
    Logger().info("WebView message handler registered");

    // 设置Virtual Host映射到前端构建目录
    auto dist_path = std::filesystem::absolute(webview_state.config.frontend_dist_path).wstring();

    // 获取ICoreWebView2_3接口用于虚拟主机映射
    wil::com_ptr<ICoreWebView2_3> webview3;
    auto query_hr = webview_state.resources.webview->QueryInterface(IID_PPV_ARGS(&webview3));
    if (SUCCEEDED(query_hr) && webview3) {
      auto mapping_hr = webview3->SetVirtualHostNameToFolderMapping(
          webview_state.config.virtual_host_name.c_str(), dist_path.c_str(),
          COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
      if (SUCCEEDED(mapping_hr)) {
        Logger().info("Virtual host mapping established: {} -> {}",
                      Utils::String::ToUtf8(webview_state.config.virtual_host_name),
                      Utils::String::ToUtf8(dist_path));
      }
    }

    // 根据编译模式选择URL
#ifdef _DEBUG
    // 开发环境：连接Vite dev server (热重载)
    webview_state.config.initial_url = webview_state.config.dev_server_url;
    Logger().info("Debug mode: Using Vite dev server at {}",
                  Utils::String::ToUtf8(webview_state.config.dev_server_url));
#else
    // 生产环境：使用构建产物
    webview_state.config.initial_url =
        L"https://" + webview_state.config.virtual_host_name + L"/index.html";
    Logger().info("Release mode: Using built frontend from resources/web");
#endif

    // 导航到选定的 URL
    webview_state.resources.webview.get()->Navigate(webview_state.config.initial_url.c_str());
    webview_state.is_ready = true;

    // 初始化RPC桥接
    Core::WebView::RpcBridge::initialize_rpc_bridge(*m_state);
    webview_state.messaging.is_rpc_ready = true;

    Logger().info("WebView2 ready, navigating to: {}",
                  Utils::String::ToUtf8(webview_state.config.initial_url));

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
  HWND m_parent_hwnd;

 public:
  EnvironmentCompletedHandler(Core::State::AppState* state, HWND parent_hwnd)
      : m_state(state), m_parent_hwnd(parent_hwnd) {}

  HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment* env) {
    if (!m_state) return E_FAIL;
    auto& webview_state = m_state->webview;

    if (FAILED(result)) {
      Logger().error("Failed to create WebView2 environment: {}", result);
      return result;
    }

    webview_state.resources.environment = env;

    // 现在，使用其自己的处理器创建控制器
    auto controller_handler = Microsoft::WRL::Make<ControllerCompletedHandler>(m_state);
    return env->CreateCoreWebView2Controller(m_parent_hwnd, controller_handler.Get());
  }
};

}  // namespace

namespace Core::WebView {

auto initialize(Core::State::AppState& state, HWND parent_hwnd)
    -> std::expected<void, std::string> {
  auto& webview_state = state.webview;

  if (webview_state.is_initialized) {
    return std::unexpected("WebView already initialized");
  }

  webview_state.window.parent_hwnd = parent_hwnd;

  // 初始化 COM (如果尚未初始化)
  HRESULT com_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(com_hr) && com_hr != RPC_E_CHANGED_MODE) {
    return std::unexpected("Failed to initialize COM");
  }

  // 使用我们的有状态处理器创建环境
  auto environment_handler = Microsoft::WRL::Make<EnvironmentCompletedHandler>(&state, parent_hwnd);
  auto hr = CreateCoreWebView2Environment(environment_handler.Get());

  if (FAILED(hr)) {
    return std::unexpected("Failed to create WebView2 environment: " + std::to_string(hr));
  }

  webview_state.is_initialized = true;
  Logger().info("WebView2 initialization started");
  return {};
}

auto show_webview(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& webview_state = state.webview;

  if (!webview_state.is_ready) {
    return std::unexpected("WebView not ready");
  }

  webview_state.resources.controller.get()->put_IsVisible(TRUE);
  webview_state.window.is_visible = true;

  Logger().info("WebView shown");
  return {};
}

auto hide_webview(Core::State::AppState& state) -> void {
  auto& webview_state = state.webview;

  if (webview_state.resources.controller) {
    webview_state.resources.controller.get()->put_IsVisible(FALSE);
    webview_state.window.is_visible = false;
    Logger().info("WebView hidden");
  }
}

auto resize_webview(Core::State::AppState& state, int width, int height) -> void {
  auto& webview_state = state.webview;

  if (webview_state.resources.controller) {
    webview_state.window.width = width;
    webview_state.window.height = height;

    RECT bounds = {webview_state.window.x, webview_state.window.y, webview_state.window.x + width,
                   webview_state.window.y + height};

    webview_state.resources.controller.get()->put_Bounds(bounds);
    Logger().debug("WebView resized to {}x{}", width, height);
  }
}

auto move_webview(Core::State::AppState& state, int x, int y) -> void {
  auto& webview_state = state.webview;

  if (webview_state.resources.controller) {
    webview_state.window.x = x;
    webview_state.window.y = y;

    RECT bounds = {x, y, x + webview_state.window.width, y + webview_state.window.height};

    webview_state.resources.controller.get()->put_Bounds(bounds);
    Logger().debug("WebView moved to ({}, {})", x, y);
  }
}

auto navigate_to_url(Core::State::AppState& state, const std::wstring& url)
    -> std::expected<void, std::string> {
  auto& webview_state = state.webview;

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

auto navigate_to_string(Core::State::AppState& state, const std::wstring& html)
    -> std::expected<void, std::string> {
  auto& webview_state = state.webview;

  if (!webview_state.is_ready) {
    return std::unexpected("WebView not ready");
  }

  auto hr = webview_state.resources.webview.get()->NavigateToString(html.c_str());
  if (FAILED(hr)) {
    return std::unexpected("Failed to navigate to HTML string");
  }

  Logger().debug("Navigating to HTML string");
  return {};
}

auto is_webview_ready(const Core::State::AppState& state) -> bool { return state.webview.is_ready; }

auto is_webview_loading(const Core::State::AppState& state) -> bool {
  return state.webview.is_loading.load();
}

auto get_current_url(const Core::State::AppState& state) -> std::wstring {
  return state.webview.resources.current_url;
}

auto open_dev_tools(Core::State::AppState& state) -> void {
  auto& webview_state = state.webview;

  if (webview_state.is_ready && webview_state.config.enable_dev_tools) {
    webview_state.resources.webview.get()->OpenDevToolsWindow();
    Logger().info("Developer tools opened");
  }
}

auto close_dev_tools(Core::State::AppState& state) -> void {
  // WebView2 没有直接的关闭开发工具的API
  // 开发工具窗口需要用户手动关闭
  Logger().info("Developer tools close requested (user must close manually)");
}

auto shutdown(Core::State::AppState& state) -> void {
  auto& webview_state = state.webview;

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
  auto& webview_state = state.webview;

  if (webview_state.is_ready) {
    std::wstring wmessage = Utils::String::FromUtf8(message);
    webview_state.resources.webview.get()->PostWebMessageAsJson(wmessage.c_str());
    Logger().debug("Posted message to WebView");
  }
}

auto register_message_handler(Core::State::AppState& state, const std::string& message_type,
                              std::function<void(const std::string&)> handler) -> void {
  auto& webview_state = state.webview;

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