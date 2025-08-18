module;

export module Features.Notifications;

import Core.State;
import std;

namespace Features::Notifications {

// 对外暴露的接口，用于触发一个新通知
export auto show_notification(Core::State::AppState& state, const std::wstring& title,
                              const std::wstring& message) -> void;

// std::string 重载版本，用于方便调用
export auto show_notification(Core::State::AppState& state, const std::string& title,
                              const std::string& message) -> void;

// 在主循环中每帧调用的核心更新函数
export auto update_notifications(Core::State::AppState& state) -> void;

}  // namespace Features::Notifications