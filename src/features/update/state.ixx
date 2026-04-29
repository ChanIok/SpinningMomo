module;

export module Features.Update.State;

import std;
import Features.Update.Types;

export namespace Features::Update::State {

struct PendingUpdateContext {
  std::filesystem::path package_path;              // 待执行更新包路径
  std::filesystem::path target_install_directory;  // 待更新目标目录
  std::filesystem::path install_log_path;          // 安装器日志路径
  bool restart = true;                             // 更新完成后是否重启
  bool is_portable = true;                         // 待执行更新是否为便携版流程
};

struct UpdateState {
  // 运行时状态
  bool is_checking = false;        // 是否正在检查更新
  bool update_available = false;   // 是否有可用更新
  std::string latest_version;      // 最新版本号
  std::string downloaded_version;  // 已下载完成的版本号
  std::string error_message;       // 错误信息

  std::filesystem::path update_script_path;            // 更新脚本路径
  std::optional<PendingUpdateContext> pending_update;  // 待处理的更新上下文

  // 安装类型
  bool is_portable = true;  // 是否为便携版（通过 portable 标记文件检测）

  // 初始化状态
  bool is_initialized = false;

  UpdateState() = default;
};

// 创建默认的更新状态
inline auto create_default_update_state() -> UpdateState {
  UpdateState state;
  state.is_initialized = false;
  return state;
}

}  // namespace Features::Update::State
