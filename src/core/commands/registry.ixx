module;

export module Core.Commands;

import std;
import Core.State;
import Core.Commands.Types;
import Vendor.Windows;

namespace Core::Commands {

// === API ===

// 调用命令
export auto invoke_command(Core::State::AppState& state, const std::string& id) -> bool;

// 获取单个命令描述符（零拷贝，只读）
export auto get_command(const Core::State::AppState& state, const std::string& id)
    -> const CommandDescriptor*;

// 获取所有命令描述符（按注册顺序）
export auto get_all_commands(const Core::State::AppState& state) -> std::vector<CommandDescriptor>;

// toggle 命令是否处于开启态（非 toggle / 未找到 / 无 get_state 时返回 false）
export auto is_toggle_on(const Core::State::AppState& state, const std::string& id) -> bool;

// 注册所有内置命令（需要在应用初始化时调用）
export auto register_builtin_commands(Core::State::AppState& state) -> void;

// 安装常驻全局键盘钩子
export auto install_keyboard_keepalive_hook(Core::State::AppState& state) -> void;

// 卸载常驻全局键盘钩子
export auto uninstall_keyboard_keepalive_hook(Core::State::AppState& state) -> void;

// === 热键管理 ===

// 注册所有命令的热键
export auto register_all_hotkeys(Core::State::AppState& state, Vendor::Windows::HWND hwnd) -> void;

// 注销所有热键
export auto unregister_all_hotkeys(Core::State::AppState& state, Vendor::Windows::HWND hwnd)
    -> void;

// 处理热键消息，返回对应的命令ID（如果找到）
export auto handle_hotkey(Core::State::AppState& state, int hotkey_id)
    -> std::optional<std::string>;

}  // namespace Core::Commands
