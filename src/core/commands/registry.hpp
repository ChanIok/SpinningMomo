#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/commands/types.hpp"
#include "core/state/app_state.hpp"

namespace core::commands {

// === API ===

// 调用命令；失败时返回可直接展示的原因（未注册 / 无动作 / 动作抛出异常）
auto invoke_command(core::AppState& state, const std::string& id)
    -> std::expected<void, std::string>;

// 把命令失败呈现给用户：记录日志并弹出带命令名称与原因的提示（需在 UI 线程调用）
auto report_command_failure(core::AppState& state, const std::string& id, const std::string& reason)
    -> void;

// 获取单个命令描述符（零拷贝，只读）
auto get_command(const core::AppState& state, const std::string& id) -> const CommandDescriptor*;

// 获取所有命令的可传输元数据（按注册顺序，不暴露内部回调）
auto get_all_commands(const core::AppState& state) -> std::vector<CommandDescriptorData>;

// toggle 命令是否处于开启态（非 toggle / 未找到 / 无 get_state 时返回 false）
auto is_toggle_on(const core::AppState& state, const std::string& id) -> bool;

// 注册所有内置命令（需要在应用初始化时调用）
auto register_builtin_commands(core::AppState& state) -> void;

// 安装常驻全局键盘钩子
auto install_keyboard_keepalive_hook(core::AppState& state) -> void;

// 卸载常驻全局键盘钩子
auto uninstall_keyboard_keepalive_hook(core::AppState& state) -> void;

// === 热键管理 ===

// 注册所有命令的热键
auto register_all_hotkeys(core::AppState& state, HWND hwnd) -> void;

// 注销所有热键
auto unregister_all_hotkeys(core::AppState& state, HWND hwnd) -> void;

// 处理热键消息，返回对应的命令ID（如果找到）
auto handle_hotkey(core::AppState& state, int hotkey_id) -> std::optional<std::string>;

}  // namespace core::commands
