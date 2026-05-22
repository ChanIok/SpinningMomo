module;

export module Core.Commands.Types;

import std;
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

}  // namespace Core::Commands
