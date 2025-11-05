module;

export module Features.Update.State;

import std;
import Features.Update.Types;

export namespace Features::Update::State {

struct UpdateState {
  // 运行时状态
  bool is_checking = false;           // 是否正在检查更新
  bool download_in_progress = false;  // 是否正在下载
  bool update_available = false;      // 是否有可用更新
  std::string latest_version;         // 最新版本号
  std::string changelog;              // 更新日志
  std::string download_url;           // 下载链接
  double download_progress = 0.0;     // 下载进度 (0.0-1.0)
  std::string error_message;          // 错误信息

  std::filesystem::path update_script_path;  // 更新脚本路径
  bool pending_update = false;               // 是否有待处理的更新

  // 当前服务器状态
  int current_server_index = 0;  // 当前使用的服务器索引

  // 初始化状态
  bool is_initialized = false;

  UpdateState() = default;
};

// 创建默认的更新状态
inline auto create_default_update_state() -> UpdateState {
  UpdateState state;
  state.current_server_index = 0;  // 默认使用第一个服务器
  state.is_initialized = false;
  return state;
}

}  // namespace Features::Update::State