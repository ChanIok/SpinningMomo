module;

export module Core.Commands;

import std;
import Core.State;
import Vendor.Windows;

namespace Core::Commands {

// 热键绑定
export struct HotkeyBinding {
  Vendor::Windows::UINT modifiers = 0;  // MOD_CONTROL=1, MOD_ALT=2, MOD_SHIFT=4
  Vendor::Windows::UINT key = 0;        // 虚拟键码 (VK_*)
  std::string settings_path;            // 设置文件中的路径，如 "app.hotkey.floating_window"
};

// 命令描述符
export struct CommandDescriptor {
  std::string id;        // 唯一标识，如 "screenshot.capture"
  std::string i18n_key;  // i18n 键，如 "menu.screenshot_capture"

  bool is_toggle = false;  // 是否为切换类型

  std::function<void()> action;               // 点击执行的动作
  std::function<bool()> get_state = nullptr;  // toggle 类型：获取当前状态

  std::optional<HotkeyBinding> hotkey;  // 热键绑定（可选）
};

// 运行时命令注册表
export struct CommandRegistry {
  std::unordered_map<std::string, CommandDescriptor> descriptors;
  std::vector<std::string> registration_order;  // 保持注册顺序
};

// === API ===

// 注册命令
export auto register_command(CommandRegistry& registry, CommandDescriptor descriptor) -> void;

// 调用命令
export auto invoke_command(CommandRegistry& registry, const std::string& id) -> bool;

// 获取所有命令描述符（按注册顺序）
export auto get_all_commands(const CommandRegistry& registry) -> std::vector<CommandDescriptor>;

// 获取单个命令描述符
export auto get_command(const CommandRegistry& registry, const std::string& id)
    -> std::optional<CommandDescriptor>;

// === RPC Types ===

// 用于 RPC 传输的命令描述符（不包含 function 字段）
export struct CommandDescriptorData {
  std::string id;
  std::string i18n_key;
  bool is_toggle;
};

export struct GetAllCommandsParams {
  // 空结构体，未来可扩展
};

export struct GetAllCommandsResult {
  std::vector<CommandDescriptorData> commands;
};

export struct InvokeCommandParams {
  std::string id;
};

export struct InvokeCommandResult {
  bool success = false;
  std::string message;
};

// 注册所有内置命令（需要在应用初始化时调用）
export auto register_builtin_commands(Core::State::AppState& state, CommandRegistry& registry)
    -> void;

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
