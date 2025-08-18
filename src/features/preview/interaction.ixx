module;

#include <windows.h>

export module Features.Preview.Interaction;

import std;
import Core.State;

namespace Features::Preview::Interaction {

// 主消息处理函数
// 返回值：first为true表示已处理消息，为false表示应使用默认处理
export auto handle_preview_message(Core::State::AppState& state, HWND hwnd, UINT message,
                                   WPARAM wParam, LPARAM lParam) -> std::pair<bool, LRESULT>;

}  // namespace Features::Preview::Interaction