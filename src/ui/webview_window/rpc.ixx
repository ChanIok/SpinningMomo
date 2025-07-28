module;

export module UI.WebViewWindow.Rpc;

import std;
import Core.State;

namespace UI::WebViewWindow::Rpc {

// RPC参数和结果类型定义
struct DragParams {
  int x;
  int y;
};

struct WindowStateResult {
  bool is_maximized;
};

struct WindowControlResult {
  bool success;
};

// 注册RPC方法
export auto register_handlers(Core::State::AppState& app_state) -> void;

}  // namespace UI::WebViewWindow::Rpc