#pragma once

#include "core/commands/types.hpp"
#include "core/state/app_state.hpp"
#include "vendor/windows.hpp"

namespace Core::Commands {

// === API ===

// 调用命令
auto invoke_command(Core::State::AppState& state, const std::string& id) -> bool;

// 获取单个命令描述符（零拷贝，只读）
auto get_command(const Core::State::AppState& state, const std::string& id)
    -> const CommandDescriptor*;

// 获取所有命令的可传输元数据（按注册顺序，不暴露内部回调）
auto get_all_commands(const Core::State::AppState& state) -> std::vector<CommandDescriptorData>;

// toggle 命令是否处于开启态（非 toggle / 未找到 / 无 get_state 时返回 false）
auto is_toggle_on(const Core::State::AppState& state, const std::string& id) -> bool;

// 注册所有内置命令（需要在应用初始化时调用）
auto register_builtin_commands(Core::State::AppState& state) -> void;

// 安装常驻全局键盘钩子
auto install_keyboard_keepalive_hook(Core::State::AppState& state) -> void;

// 卸载常驻全局键盘钩子
auto uninstall_keyboard_keepalive_hook(Core::State::AppState& state) -> void;

// === 热键管理 ===

// 注册所有命令的热键
auto register_all_hotkeys(Core::State::AppState& state, Vendor::Windows::HWND hwnd) -> void;

// 注销所有热键
auto unregister_all_hotkeys(Core::State::AppState& state, Vendor::Windows::HWND hwnd) -> void;

// 处理热键消息，返回对应的命令ID（如果找到）
auto handle_hotkey(Core::State::AppState& state, int hotkey_id) -> std::optional<std::string>;

}  // namespace Core::Commands
