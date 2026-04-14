module;

#include <wil/com.h>

#include <WebView2.h>  // 必须放最后面

export module Core.WebView.State;

import std;
import Core.WebView.Types;
import <d3d11.h>;
import <dcomp.h>;
import <windows.h>;

namespace Core::WebView::State {

export constexpr UINT kWM_APP_BEGIN_RESIZE = WM_APP + 2;
// WM_APP + 3：通知 WebView 窗口线程对虚拟主机映射进行协调同步
// 由 register/unregister_virtual_host_folder_mapping 触发，实际执行在窗口消息循环中
export constexpr UINT kWM_APP_RECONCILE_VIRTUAL_HOST_MAPPINGS = WM_APP + 3;

// Composition Hosting 运行时资源
export struct HostRuntime {
  wil::com_ptr<ID3D11Device> d3d_device;
  wil::com_ptr<IDCompositionDevice> dcomp_device;
  wil::com_ptr<IDCompositionTarget> dcomp_target;
  wil::com_ptr<IDCompositionVisual> dcomp_root_visual;
};

// WebView窗口状态
export struct WindowState {
  HWND webview_hwnd = nullptr;
  int width = 900;
  int height = 600;
  int x = 0;
  int y = 0;
  bool is_visible = false;
  bool is_maximized = false;
  bool is_fullscreen = false;
  bool has_fullscreen_restore_state = false;
  DWORD fullscreen_restore_style = 0;
  WINDOWPLACEMENT fullscreen_restore_placement{sizeof(WINDOWPLACEMENT)};
};

// WebView核心资源
export enum class VirtualHostResourceAccessKind {
  deny = COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY,
  allow = COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW,
  deny_cors = COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS,
};

export struct VirtualHostFolderMapping {
  std::wstring folder_path;
  VirtualHostResourceAccessKind access_kind = VirtualHostResourceAccessKind::deny_cors;
};

export struct DocumentCreatedScript {
  std::string id;
  std::wstring script;
};

export struct CoreResources {
  wil::com_ptr<ICoreWebView2Environment> environment;
  wil::com_ptr<ICoreWebView2Controller> controller;
  wil::com_ptr<ICoreWebView2CompositionController> composition_controller;
  wil::com_ptr<ICoreWebView2CompositionController4> composition_controller4;
  wil::com_ptr<ICoreWebView2> webview;

  // 事件注册令牌，用于清理时取消注册
  EventRegistrationToken navigation_starting_token{};
  EventRegistrationToken navigation_completed_token{};
  EventRegistrationToken new_window_requested_token{};
  EventRegistrationToken web_message_received_token{};
  std::vector<EventRegistrationToken> webresource_requested_tokens;

  std::wstring user_data_folder;
  std::wstring current_url;

  // WebView 资源解析器注册表
  std::unique_ptr<Types::WebResolverRegistry> web_resolvers;
  std::unordered_map<std::wstring, VirtualHostFolderMapping> virtual_host_folder_mappings;
  // 已通过 WebView2 COM 接口实际设置的虚拟主机映射集合
  // 与 virtual_host_folder_mappings（期望状态）共同用于 reconcile 时的差量计算
  std::unordered_set<std::wstring> applied_virtual_host_folder_mappings;
  std::mutex virtual_host_folder_mappings_mutex;
  std::unordered_map<std::string, DocumentCreatedScript> document_created_scripts;
  std::mutex document_created_scripts_mutex;
  HostRuntime host_runtime;

  // 构造函数：初始化解析器注册表
  CoreResources() : web_resolvers(std::make_unique<Types::WebResolverRegistry>()) {}
};

// 消息通信状态
export struct MessageState {
  std::queue<std::string> pending_messages;
  std::unordered_map<std::string, std::string> message_responses;
  std::unordered_map<std::string, std::function<void(const std::string&)>> handlers;
  std::mutex message_mutex;
  std::atomic<uint64_t> next_message_id{0};
};

// WebView配置
export struct WebViewConfig {
  std::wstring user_data_folder = L"webview2";
  std::wstring initial_url = L"";  // 运行时根据编译模式自动设置
  std::vector<std::wstring> allowed_origins;
  bool enable_password_autosave = false;
  bool enable_general_autofill = false;

  // 前端加载配置
  std::wstring frontend_dist_path = L"./resources/web";    // 前端构建产物路径
  std::wstring virtual_host_name = L"app.test";            // 前端虚拟主机名
  std::wstring static_host_name = L"static.test";          // 通用静态资源路径
  std::wstring thumbnail_host_name = L"thumbs.test";       // 缩略图虚拟主机名
  std::wstring dev_server_url = L"http://localhost:5173";  // 开发服务器URL
};

// WebView完整状态
export struct WebViewState {
  WindowState window;
  CoreResources resources;
  MessageState messaging;
  WebViewConfig config;

  bool is_initialized = false;
  bool is_ready = false;
  bool has_initial_content = false;
};

}  // namespace Core::WebView::State
