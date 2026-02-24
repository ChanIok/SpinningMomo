module;

#include <wil/com.h>

#include <WebView2.h>  // 必须放最后面

module Core.WebView.Host;

import std;
import Core.State;
import Core.WebView.RpcBridge;
import Core.WebView.State;
import Core.WebView.Static;
import Features.Settings.State;
import Utils.Logger;
import Utils.String;
import Vendor.BuildConfig;
import Vendor.WIL;
import <d3d11.h>;
import <dcomp.h>;
import <dxgi.h>;
import <windows.h>;
import <wrl.h>;

namespace Core::WebView::Host::Detail {

auto clear_host_runtime(Core::WebView::State::HostRuntime& host_runtime) -> void {
  host_runtime.dcomp_root_visual.reset();
  host_runtime.dcomp_target.reset();
  host_runtime.dcomp_device.reset();
  host_runtime.d3d_device.reset();
}

auto create_composition_host(HWND hwnd, Core::WebView::State::HostRuntime& host_runtime)
    -> std::expected<void, std::string> {
  clear_host_runtime(host_runtime);

  UINT d3d_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
  d3d_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  wil::com_ptr<ID3D11DeviceContext> d3d_context;
  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, d3d_flags, nullptr, 0,
                                 D3D11_SDK_VERSION, host_runtime.d3d_device.put(), nullptr,
                                 d3d_context.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to create D3D11 device for WebView composition: 0x{:08X}",
                    static_cast<unsigned>(hr)));
  }

  wil::com_ptr<IDXGIDevice> dxgi_device;
  hr = host_runtime.d3d_device->QueryInterface(IID_PPV_ARGS(&dxgi_device));
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to query IDXGIDevice for WebView composition: 0x{:08X}",
                    static_cast<unsigned>(hr)));
  }

  hr = DCompositionCreateDevice(dxgi_device.get(), IID_PPV_ARGS(&host_runtime.dcomp_device));
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to create DComposition device: 0x{:08X}", static_cast<unsigned>(hr)));
  }

  hr = host_runtime.dcomp_device->CreateTargetForHwnd(hwnd, TRUE, host_runtime.dcomp_target.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to create DComposition target for WebView window: 0x{:08X}",
                    static_cast<unsigned>(hr)));
  }

  hr = host_runtime.dcomp_device->CreateVisual(host_runtime.dcomp_root_visual.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create DComposition root visual: 0x{:08X}",
                                       static_cast<unsigned>(hr)));
  }

  hr = host_runtime.dcomp_target->SetRoot(host_runtime.dcomp_root_visual.get());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to set DComposition root visual: 0x{:08X}", static_cast<unsigned>(hr)));
  }

  hr = host_runtime.dcomp_device->Commit();
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to commit initial DComposition tree: 0x{:08X}",
                                       static_cast<unsigned>(hr)));
  }

  return {};
}

auto is_system_light_theme() -> bool {
  DWORD value = 0;
  DWORD value_size = sizeof(value);
  auto status = RegGetValueW(HKEY_CURRENT_USER,
                             L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                             L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &value_size);

  if (status != ERROR_SUCCESS) {
    Logger().warn("Failed to read system theme from registry, fallback to dark mode");
    return false;
  }

  return value != 0;
}

auto resolve_opaque_background_color(std::string_view theme_mode) -> COREWEBVIEW2_COLOR {
  // Match web/src/index.css surface-bottom colors for light/dark themes.
  // COREWEBVIEW2_COLOR field order is A, R, G, B.
  constexpr COREWEBVIEW2_COLOR light_color{255, 236, 238, 242};  // #ECEEF2
  constexpr COREWEBVIEW2_COLOR dark_color{255, 23, 24, 26};      // #17181A

  if (theme_mode == "light") return light_color;
  if (theme_mode == "dark") return dark_color;

  return is_system_light_theme() ? light_color : dark_color;
}

auto read_background_mode(Core::State::AppState& state) -> std::pair<bool, std::string> {
  bool transparent_enabled = false;
  std::string theme_mode = "system";
  if (state.settings) {
    transparent_enabled = state.settings->raw.ui.webview_window.enable_transparent_background;
    theme_mode = state.settings->raw.ui.web_theme.mode;
  }
  return {transparent_enabled, std::move(theme_mode)};
}

auto use_composition_host_from_settings(Core::State::AppState& state) -> bool {
  if (!state.settings) {
    return false;
  }
  return state.settings->raw.ui.webview_window.enable_transparent_background;
}

auto apply_default_background(ICoreWebView2Controller* controller, bool transparent_enabled,
                              std::string_view theme_mode) -> void {
  wil::com_ptr<ICoreWebView2Controller2> controller2;
  if (FAILED(controller->QueryInterface(IID_PPV_ARGS(&controller2))) || !controller2) {
    Logger().warn("ICoreWebView2Controller2 unavailable, WebView background not applied");
    return;
  }

  auto color = transparent_enabled ? COREWEBVIEW2_COLOR{0, 0, 0, 0}
                                   : resolve_opaque_background_color(theme_mode);
  auto hr = controller2->put_DefaultBackgroundColor(color);
  if (FAILED(hr)) {
    Logger().warn("Failed to apply WebView default background color: {}", hr);
    return;
  }

  Logger().info("WebView2 default background updated: transparent={}, alpha={}",
                transparent_enabled ? "true" : "false", static_cast<int>(color.A));
}

class NavigationStartingEventHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ICoreWebView2NavigationStartingEventHandler> {
 private:
  Core::State::AppState* m_state;

 public:
  explicit NavigationStartingEventHandler(Core::State::AppState* state) : m_state(state) {}

  HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender,
                                   ICoreWebView2NavigationStartingEventArgs* args) {
    (void)sender;
    (void)args;
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
  explicit NavigationCompletedEventHandler(Core::State::AppState* state) : m_state(state) {}

  HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender,
                                   ICoreWebView2NavigationCompletedEventArgs* args) {
    (void)sender;
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

auto setup_navigation_events(Core::State::AppState* state, ICoreWebView2* webview,
                             Core::WebView::State::CoreResources& resources) -> HRESULT {
  if (!state || !webview) return E_FAIL;

  auto navigation_starting_handler = Microsoft::WRL::Make<NavigationStartingEventHandler>(state);
  HRESULT hr = webview->add_NavigationStarting(navigation_starting_handler.Get(),
                                               &resources.navigation_starting_token);
  if (FAILED(hr)) {
    Logger().error("Failed to register NavigationStarting event: {}", hr);
    return hr;
  }

  auto navigation_completed_handler = Microsoft::WRL::Make<NavigationCompletedEventHandler>(state);
  hr = webview->add_NavigationCompleted(navigation_completed_handler.Get(),
                                        &resources.navigation_completed_token);
  if (FAILED(hr)) {
    Logger().error("Failed to register NavigationCompleted event: {}", hr);
    return hr;
  }

  Logger().debug("Navigation events registered successfully");
  return S_OK;
}

auto setup_message_handler(Core::State::AppState* state, ICoreWebView2* webview,
                           Core::WebView::State::CoreResources& resources) -> HRESULT {
  if (!state || !webview) return E_FAIL;

  auto message_handler = Core::WebView::RpcBridge::create_message_handler(*state);

  auto webview_message_handler =
      Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
          [message_handler](ICoreWebView2* sender,
                            ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
            (void)sender;
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

auto setup_virtual_host_mapping(ICoreWebView2* webview, Core::WebView::State::WebViewConfig& config)
    -> HRESULT {
  if (!webview) return E_FAIL;

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
                Utils::String::ToUtf8(config.virtual_host_name), Utils::String::ToUtf8(dist_path));
  return S_OK;
}

auto select_initial_url(Core::WebView::State::WebViewConfig& config) -> void {
  if (Vendor::BuildConfig::is_debug_build()) {
    config.initial_url = config.dev_server_url;
    Logger().info("Debug mode: Using Vite dev server at {}",
                  Utils::String::ToUtf8(config.dev_server_url));
  } else {
    config.initial_url = L"https://" + config.virtual_host_name + L"/index.html";
    Logger().info("Release mode: Using built frontend from resources/web");
  }
}

auto enable_non_client_region_support(ICoreWebView2* webview) -> bool {
  if (!webview) return false;

  wil::com_ptr<ICoreWebView2Settings> settings;
  auto hr = webview->get_Settings(settings.put());
  if (FAILED(hr) || !settings) {
    Logger().warn("Failed to get WebView settings, non-client region support disabled");
    return false;
  }

  wil::com_ptr<ICoreWebView2Settings9> settings9;
  hr = settings->QueryInterface(IID_PPV_ARGS(&settings9));
  if (FAILED(hr) || !settings9) {
    Logger().warn("ICoreWebView2Settings9 unavailable, non-client region support disabled");
    return false;
  }

  hr = settings9->put_IsNonClientRegionSupportEnabled(TRUE);
  if (FAILED(hr)) {
    Logger().warn("Failed to enable non-client region support: {}", hr);
    return false;
  }

  Logger().info("WebView non-client region support enabled");
  return true;
}

auto setup_composition_non_client_support(
    ICoreWebView2CompositionController* composition_controller,
    Core::WebView::State::CoreResources& resources) -> void {
  if (!composition_controller) {
    resources.composition_controller4.reset();
    return;
  }

  auto hr =
      composition_controller->QueryInterface(IID_PPV_ARGS(resources.composition_controller4.put()));
  if (FAILED(hr) || !resources.composition_controller4) {
    Logger().warn("ICoreWebView2CompositionController4 unavailable, composition hit-test disabled");
    resources.composition_controller4.reset();
    return;
  }

  Logger().info("Composition non-client region interface enabled");
}

auto initialize_navigation(ICoreWebView2* webview, const std::wstring& initial_url) -> HRESULT {
  HRESULT hr = webview->Navigate(initial_url.c_str());
  if (FAILED(hr)) {
    Logger().error("Failed to navigate to initial URL: {}", hr);
    return hr;
  }

  Logger().info("WebView2 ready, navigating to: {}", Utils::String::ToUtf8(initial_url));
  return S_OK;
}

auto finalize_controller_initialization(Core::State::AppState* state,
                                        ICoreWebView2Controller* controller,
                                        ICoreWebView2CompositionController* composition_controller)
    -> HRESULT {
  if (!state || !controller) return E_FAIL;

  auto& webview_state = *state->webview;
  auto& resources = webview_state.resources;

  resources.navigation_starting_token = {};
  resources.navigation_completed_token = {};
  resources.web_message_received_token = {};
  resources.webresource_requested_tokens.clear();
  resources.webview.reset();

  resources.controller = controller;
  resources.composition_controller = composition_controller;
  if (!composition_controller) {
    resources.composition_controller4.reset();
    clear_host_runtime(resources.host_runtime);
  }

  HRESULT hr = resources.controller->get_CoreWebView2(resources.webview.put());
  if (FAILED(hr)) {
    Logger().error("Failed to get CoreWebView2: {}", hr);
    return hr;
  }

  if (composition_controller) {
    auto& host_runtime = resources.host_runtime;
    if (!host_runtime.dcomp_root_visual || !host_runtime.dcomp_device) {
      Logger().error("Composition host runtime is not initialized");
      return E_FAIL;
    }

    hr = composition_controller->put_RootVisualTarget(host_runtime.dcomp_root_visual.get());
    if (FAILED(hr)) {
      Logger().error("Failed to set composition root visual target: {}", hr);
      return hr;
    }

    hr = host_runtime.dcomp_device->Commit();
    if (FAILED(hr)) {
      Logger().error("Failed to commit composition visual tree: {}", hr);
      return hr;
    }
  }

  auto* webview = resources.webview.get();
  auto* environment = resources.environment.get();

  RECT bounds = {0, 0, webview_state.window.width, webview_state.window.height};
  resources.controller->put_Bounds(bounds);

  auto [transparent_enabled, theme_mode] = read_background_mode(*state);
  apply_default_background(resources.controller.get(), transparent_enabled, theme_mode);

  hr = setup_navigation_events(state, webview, resources);
  if (FAILED(hr)) return hr;

  hr = setup_message_handler(state, webview, resources);
  if (FAILED(hr)) return hr;

  hr = setup_virtual_host_mapping(webview, webview_state.config);
  if (FAILED(hr)) {
    if (Vendor::BuildConfig::is_debug_build()) {
      Logger().warn("Virtual host mapping failed in debug mode, continuing with dev server");
    } else {
      return hr;
    }
  }

  hr = Core::WebView::Static::setup_resource_interception(*state, webview, environment, resources,
                                                          webview_state.config);
  if (FAILED(hr)) {
    Logger().warn("Resource interception setup failed, continuing without thumbnail support");
  }

  select_initial_url(webview_state.config);

  auto non_client_enabled = enable_non_client_region_support(webview);
  if (composition_controller && non_client_enabled) {
    setup_composition_non_client_support(composition_controller, resources);
  } else {
    resources.composition_controller4.reset();
  }

  hr = initialize_navigation(webview, webview_state.config.initial_url);
  if (FAILED(hr)) return hr;

  webview_state.is_ready = true;
  Core::WebView::RpcBridge::initialize_rpc_bridge(*state);

  return S_OK;
}

class CompositionControllerCompletedHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler> {
 private:
  Core::State::AppState* m_state;

 public:
  explicit CompositionControllerCompletedHandler(Core::State::AppState* state) : m_state(state) {}

  HRESULT STDMETHODCALLTYPE Invoke(HRESULT result,
                                   ICoreWebView2CompositionController* composition_controller) {
    if (!m_state) return E_FAIL;
    if (FAILED(result)) {
      Logger().error("Failed to create WebView2 composition controller: {}", result);
      return result;
    }
    if (!composition_controller) {
      return E_POINTER;
    }

    wil::com_ptr<ICoreWebView2Controller> controller;
    auto hr = composition_controller->QueryInterface(IID_PPV_ARGS(&controller));
    if (FAILED(hr) || !controller) {
      Logger().error("Failed to cast composition controller to base controller: {}", hr);
      return FAILED(hr) ? hr : E_NOINTERFACE;
    }

    return finalize_controller_initialization(m_state, controller.get(), composition_controller);
  }
};

class HwndControllerCompletedHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> {
 private:
  Core::State::AppState* m_state;

 public:
  explicit HwndControllerCompletedHandler(Core::State::AppState* state) : m_state(state) {}

  HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller* controller) {
    if (!m_state) return E_FAIL;
    if (FAILED(result)) {
      Logger().error("Failed to create WebView2 HWND controller: {}", result);
      return result;
    }

    return finalize_controller_initialization(m_state, controller, nullptr);
  }
};

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

    if (use_composition_host_from_settings(*m_state)) {
      Logger().info("WebView host mode resolved to Composition");

      auto create_host_result =
          create_composition_host(m_webview_hwnd, webview_state.resources.host_runtime);
      if (!create_host_result) {
        Logger().error("Failed to initialize composition host: {}", create_host_result.error());
        return E_FAIL;
      }

      wil::com_ptr<ICoreWebView2Environment3> env3;
      auto hr = env->QueryInterface(IID_PPV_ARGS(&env3));
      if (FAILED(hr) || !env3) {
        Logger().error(
            "ICoreWebView2Environment3 is unavailable; composition hosting not supported");
        clear_host_runtime(webview_state.resources.host_runtime);
        return FAILED(hr) ? hr : E_NOINTERFACE;
      }

      auto controller_handler =
          Microsoft::WRL::Make<CompositionControllerCompletedHandler>(m_state);
      hr = env3->CreateCoreWebView2CompositionController(m_webview_hwnd, controller_handler.Get());
      if (FAILED(hr)) {
        Logger().error("Failed to start composition controller creation: {}", hr);
        clear_host_runtime(webview_state.resources.host_runtime);
      }
      return hr;
    }

    Logger().info("WebView host mode resolved to HWND controller");
    clear_host_runtime(webview_state.resources.host_runtime);
    webview_state.resources.composition_controller.reset();
    webview_state.resources.composition_controller4.reset();

    auto controller_handler = Microsoft::WRL::Make<HwndControllerCompletedHandler>(m_state);
    auto hr = env->CreateCoreWebView2Controller(m_webview_hwnd, controller_handler.Get());
    if (FAILED(hr)) {
      Logger().error("Failed to start HWND controller creation: {}", hr);
    }
    return hr;
  }
};

}  // namespace Core::WebView::Host::Detail

namespace Core::WebView::Host {

auto start_environment_creation(Core::State::AppState& state, HWND webview_hwnd)
    -> std::expected<void, std::string> {
  try {
    auto environment_handler =
        Microsoft::WRL::Make<Detail::EnvironmentCompletedHandler>(&state, webview_hwnd);

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
                                                          environment_handler.Get());
    Vendor::WIL::throw_if_failed(hr);

    return {};
  } catch (const wil::ResultException& e) {
    auto error_msg =
        std::format("Failed to initialize WebView2 environment: {} (HRESULT: 0x{:08X})", e.what(),
                    static_cast<unsigned>(e.GetErrorCode()));
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  } catch (const std::exception& e) {
    auto error_msg =
        std::format("Unexpected error during WebView2 environment creation: {}", e.what());
    Logger().error(error_msg);
    return std::unexpected(error_msg);
  }
}

auto reset_host_runtime(Core::State::AppState& state) -> void {
  if (!state.webview) return;
  Detail::clear_host_runtime(state.webview->resources.host_runtime);
}

auto apply_background_mode_from_settings(Core::State::AppState& state) -> void {
  auto* controller = state.webview->resources.controller.get();
  if (!controller) {
    Logger().debug("Skip applying WebView background mode: controller is not ready");
    return;
  }

  auto [transparent_enabled, theme_mode] = Detail::read_background_mode(state);
  Detail::apply_default_background(controller, transparent_enabled, theme_mode);
}

}  // namespace Core::WebView::Host
