module;

#include <windows.h>

export module Core.WebView.DragHandler;

import std;
import Core.State;
import Vendor.Windows;

namespace Core::WebView::DragHandler {
    // 注册拖动处理对象到WebView
    export auto register_drag_handler(Core::State::AppState& state) -> std::expected<void, std::string>;
}