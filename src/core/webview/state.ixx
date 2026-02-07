module;

#include <wil/com.h>

#include <WebView2.h>  // 必须放最后面
export module Core.WebView.State;

import std;
import Core.WebView.Types;
import <windows.h>;

export namespace Core::WebView::State {

// WebView窗口状态
struct WindowState {
  HWND webview_hwnd = nullptr;
  int width = 1200;
  int height = 800;
  int x = 0;
  int y = 0;
  bool is_visible = false;
};

// WebView核心资源
struct CoreResources {
  wil::com_ptr<ICoreWebView2Environment> environment;
  wil::com_ptr<ICoreWebView2Controller> controller;
  wil::com_ptr<ICoreWebView2> webview;

  // 事件注册令牌，用于清理时取消注册
  EventRegistrationToken navigation_starting_token{};
  EventRegistrationToken navigation_completed_token{};
  EventRegistrationToken web_message_received_token{};
  std::vector<EventRegistrationToken> webresource_requested_tokens;

  std::wstring user_data_folder;
  std::wstring current_url;

  // WebView 资源解析器注册表
  std::unique_ptr<Types::WebResolverRegistry> web_resolvers;

  // 构造函数：初始化解析器注册表
  CoreResources() : web_resolvers(std::make_unique<Types::WebResolverRegistry>()) {}
};

// 消息通信状态
struct MessageState {
  std::queue<std::string> pending_messages;
  std::unordered_map<std::string, std::string> message_responses;
  std::unordered_map<std::string, std::function<void(const std::string&)>> handlers;
  std::mutex message_mutex;
  std::atomic<uint64_t> next_message_id{0};
};

// WebView配置
struct WebViewConfig {
  std::wstring user_data_folder = L"./webview_data";
  std::wstring initial_url = L"";  // 运行时根据编译模式自动设置
  std::vector<std::wstring> allowed_origins;
  bool enable_password_autosave = false;
  bool enable_general_autofill = false;

  // 前端加载配置
  std::wstring frontend_dist_path = L"./resources/web";    // 前端构建产物路径
  std::wstring virtual_host_name = L"app.local";           // 前端虚拟主机名
  std::wstring static_host_name = L"static.app.local";     // 静态资源路径
  std::wstring dev_server_url = L"http://localhost:5173";  // 开发服务器URL
};

// WebView完整状态
struct WebViewState {
  WindowState window;
  CoreResources resources;
  MessageState messaging;
  WebViewConfig config;

  bool is_initialized = false;
  bool is_ready = false;
};

}  // namespace Core::WebView::State