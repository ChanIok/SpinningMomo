module;

#include <wil/com.h>

#include <WebView2.h>  // 必须放最后面

module Core.WebView;

import std;
import Core.State;
import Core.WebView.Host;
import Core.WebView.State;
import Utils.Logger;
import Utils.String;
import <windows.h>;
import <windowsx.h>;

namespace Core::WebView::Detail {

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

}  // namespace Core::WebView::Detail

namespace Core::WebView {

auto get_runtime_version() -> std::expected<std::string, std::string> {
  LPWSTR version_raw = nullptr;
  auto hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version_raw);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("GetAvailableCoreWebView2BrowserVersionString failed: 0x{:08X}",
                    static_cast<unsigned>(hr)));
  }

  if (!version_raw) {
    return std::unexpected("WebView2 runtime version string is empty");
  }

  std::string version = Utils::String::ToUtf8(version_raw);
  CoTaskMemFree(version_raw);
  return version;
}

auto initialize(Core::State::AppState& state, HWND webview_hwnd)
    -> std::expected<void, std::string> {
  auto& webview_state = *state.webview;

  if (webview_state.is_initialized) {
    return std::unexpected("WebView already initialized");
  }

  try {
    // 使用 wil RAII 初始化 COM，static 确保整个应用生命周期内有效
    static auto coinit = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
    (void)coinit;

    auto init_result = Core::WebView::Host::start_environment_creation(state, webview_hwnd);
    if (!init_result) {
      return std::unexpected(init_result.error());
    }

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

  if (webview_state.resources.controller) {
    webview_state.resources.controller.get()->Close();
  }

  webview_state.resources.webview.reset();
  webview_state.resources.composition_controller4.reset();
  webview_state.resources.composition_controller.reset();
  webview_state.resources.controller.reset();
  webview_state.resources.environment.reset();
  Core::WebView::Host::reset_host_runtime(state);

  webview_state.is_initialized = false;
  webview_state.is_ready = false;
  webview_state.window.is_visible = false;

  Logger().info("WebView shutdown completed");
}

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

auto apply_background_mode_from_settings(Core::State::AppState& state) -> void {
  Core::WebView::Host::apply_background_mode_from_settings(state);
}

auto is_composition_active(Core::State::AppState& state) -> bool {
  return state.webview->resources.composition_controller != nullptr;
}

auto forward_mouse_message(Core::State::AppState& state, HWND hwnd, UINT msg, WPARAM wparam,
                           LPARAM lparam) -> bool {
  auto& webview_state = *state.webview;
  auto* composition_controller = webview_state.resources.composition_controller.get();
  if (!composition_controller) {
    return false;
  }

  auto event_kind = Detail::to_mouse_event_kind(msg);
  if (!event_kind) {
    return false;
  }

  COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS keys = Detail::to_mouse_virtual_keys(wparam);

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

  return Detail::to_non_client_hit_test(region_kind);
}

auto send_message(Core::State::AppState& state, const std::string& message)
    -> std::expected<std::string, std::string> {
  // 这是一个简化版本，实际应该实现完整的请求-响应机制
  post_message(state, message);
  return std::string("Message sent");
}

}  // namespace Core::WebView
