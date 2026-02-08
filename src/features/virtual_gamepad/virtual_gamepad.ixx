module;

export module Features.VirtualGamepad;

import std;
import Core.State;

namespace Features::VirtualGamepad {

// 初始化：连接 ViGEm，检测驱动可用性（不自动启用）
export auto initialize(Core::State::AppState& state) -> std::expected<void, std::string>;

// 清理：释放所有资源
export auto shutdown(Core::State::AppState& state) -> void;

// 启用：创建虚拟手柄、安装键盘钩子、启动更新循环
export auto enable(Core::State::AppState& state) -> std::expected<void, std::string>;

// 禁用：停止更新循环、卸载键盘钩子、释放虚拟手柄
export auto disable(Core::State::AppState& state) -> void;

// 切换启用/禁用
export auto toggle(Core::State::AppState& state) -> std::expected<void, std::string>;

// 检查 ViGEm 是否可用
export auto is_available(const Core::State::AppState& state) -> bool;

// 检查是否已启用
export auto is_enabled(const Core::State::AppState& state) -> bool;

}  // namespace Features::VirtualGamepad
