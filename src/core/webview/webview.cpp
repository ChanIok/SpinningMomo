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

namespace Detail {

auto snapshot_virtual_host_folder_mappings(Core::State::AppState& state)
    -> std::pair<std::unordered_map<std::wstring, Core::WebView::State::VirtualHostFolderMapping>,
                 std::unordered_set<std::wstring>> {
  auto& resources = state.webview->resources;

  std::lock_guard<std::mutex> lock(resources.virtual_host_folder_mappings_mutex);
  return {resources.virtual_host_folder_mappings, resources.applied_virtual_host_folder_mappings};
}

auto store_applied_virtual_host_folder_mappings(Core::State::AppState& state,
                                                std::unordered_set<std::wstring> applied_hosts)
    -> void {
  auto& resources = state.webview->resources;

  std::lock_guard<std::mutex> lock(resources.virtual_host_folder_mappings_mutex);
  resources.applied_virtual_host_folder_mappings = std::move(applied_hosts);
}

auto clear_applied_virtual_host_folder_mappings(Core::State::AppState& state) -> void {
  auto& resources = state.webview->resources;

  std::lock_guard<std::mutex> lock(resources.virtual_host_folder_mappings_mutex);
  resources.applied_virtual_host_folder_mappings.clear();
}

}  // namespace Detail

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
    webview_state.has_initial_content = false;
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
  Detail::clear_applied_virtual_host_folder_mappings(state);
  Core::WebView::Host::reset_host_runtime(state);

  webview_state.is_initialized = false;
  webview_state.is_ready = false;
  webview_state.has_initial_content = false;
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

auto register_document_created_script(Core::State::AppState& state, std::string script_id,
                                      std::wstring script_source) -> void {
  if (!state.webview) {
    return;
  }

  auto& resources = state.webview->resources;
  {
    std::lock_guard<std::mutex> lock(resources.document_created_scripts_mutex);
    resources.document_created_scripts[script_id] = Core::WebView::State::DocumentCreatedScript{
        .id = std::move(script_id),
        .script = std::move(script_source),
    };
  }

  Logger().debug("Registered WebView document-created script");
}

// 注册一条“虚拟 host -> 本地目录”的映射。
// 这里只登记状态；真正的 WebView COM 调用必须统一回到 WebView 所在线程执行。
auto register_virtual_host_folder_mapping(
    Core::State::AppState& state, std::wstring host_name, std::wstring folder_path,
    Core::WebView::State::VirtualHostResourceAccessKind access_kind) -> void {
  auto& resources = state.webview->resources;
  {
    std::lock_guard<std::mutex> lock(resources.virtual_host_folder_mappings_mutex);
    resources.virtual_host_folder_mappings[host_name] =
        Core::WebView::State::VirtualHostFolderMapping{.folder_path = folder_path,
                                                       .access_kind = access_kind};
  }

  Logger().debug("Queued WebView virtual host mapping: {} -> {}", Utils::String::ToUtf8(host_name),
                 Utils::String::ToUtf8(folder_path));
  request_virtual_host_folder_mapping_reconcile(state);
}

// 注销一条虚拟 host 映射。
// root watch 被移除时，需要把对应的 r-<rootId>.test 一并移除，避免旧 URL 继续生效。
auto unregister_virtual_host_folder_mapping(Core::State::AppState& state,
                                            std::wstring_view host_name) -> void {
  auto& resources = state.webview->resources;
  {
    std::lock_guard<std::mutex> lock(resources.virtual_host_folder_mappings_mutex);
    resources.virtual_host_folder_mappings.erase(std::wstring(host_name));
  }

  Logger().debug("Removed queued WebView virtual host mapping: {}",
                 Utils::String::ToUtf8(std::wstring(host_name)));
  request_virtual_host_folder_mapping_reconcile(state);
}

// 请求协调虚拟主机映射。
// 通过 PostMessage 将任务发布到 WebView 窗口消息队列，确保 WebView COM 调用在正确的线程上执行。
auto request_virtual_host_folder_mapping_reconcile(Core::State::AppState& state) -> void {
  if (!state.webview) {
    return;
  }

  auto hwnd = state.webview->window.webview_hwnd;
  if (!hwnd) {
    return;
  }

  if (!PostMessageW(hwnd, Core::WebView::State::kWM_APP_RECONCILE_VIRTUAL_HOST_MAPPINGS, 0, 0)) {
    Logger().debug("Skipped WebView virtual host mapping reconcile request: hwnd={} err={}",
                   reinterpret_cast<std::uintptr_t>(hwnd), GetLastError());
  }
}

// 执行虚拟主机映射协调。
// 比对 desired_mappings（期望状态）与 applied_hosts（已应用状态），执行差量更新：
// - 已应用但不在期望中的映射调用 ClearVirtualHostNameToFolderMapping 清除
// - 期望中但未应用的映射调用 SetVirtualHostNameToFolderMapping 添加
// - 已应用且仍在期望中的映射保持不变
auto reconcile_virtual_host_folder_mappings(Core::State::AppState& state) -> void {
  if (!state.webview) {
    return;
  }

  auto* webview = state.webview->resources.webview.get();
  if (!webview) {
    Detail::clear_applied_virtual_host_folder_mappings(state);
    return;
  }

  auto [desired_mappings, applied_hosts] = Detail::snapshot_virtual_host_folder_mappings(state);

  wil::com_ptr<ICoreWebView2_3> webview3;
  auto hr = webview->QueryInterface(IID_PPV_ARGS(&webview3));
  if (FAILED(hr) || !webview3) {
    Logger().warn("Failed to query ICoreWebView2_3 for virtual host mapping reconcile: {}", hr);
    return;
  }

  auto next_applied_hosts = applied_hosts;

  for (const auto& applied_host : applied_hosts) {
    if (desired_mappings.contains(applied_host)) {
      continue;
    }

    hr = webview3->ClearVirtualHostNameToFolderMapping(applied_host.c_str());
    if (FAILED(hr)) {
      Logger().warn("Failed to clear WebView virtual host mapping {}: {}",
                    Utils::String::ToUtf8(applied_host), hr);
      continue;
    }

    next_applied_hosts.erase(applied_host);
    Logger().info("Cleared WebView virtual host mapping: {}", Utils::String::ToUtf8(applied_host));
  }

  for (const auto& [host_name, mapping] : desired_mappings) {
    hr = webview3->SetVirtualHostNameToFolderMapping(
        host_name.c_str(), mapping.folder_path.c_str(),
        static_cast<COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND>(mapping.access_kind));
    if (FAILED(hr)) {
      Logger().warn("Failed to apply WebView virtual host mapping {} -> {}: {}",
                    Utils::String::ToUtf8(host_name), Utils::String::ToUtf8(mapping.folder_path),
                    hr);
      continue;
    }

    next_applied_hosts.insert(host_name);
    Logger().info("Applied WebView virtual host mapping: {} -> {}",
                  Utils::String::ToUtf8(host_name), Utils::String::ToUtf8(mapping.folder_path));
  }

  Detail::store_applied_virtual_host_folder_mappings(state, std::move(next_applied_hosts));
}

auto apply_background_mode_from_settings(Core::State::AppState& state) -> void {
  Core::WebView::Host::apply_background_mode_from_settings(state);
}

auto get_loading_background_color(Core::State::AppState& state) -> COLORREF {
  return Core::WebView::Host::get_loading_background_color(state);
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
