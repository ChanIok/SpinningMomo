module;

export module Plugins.InfinityNikki.Types;

import std;

export namespace Plugins::InfinityNikki {

struct InfinityNikkiGameDirRequest {
  // 空请求体，保持简单
};

struct InfinityNikkiGameDirResult {
  std::optional<std::string> game_dir;  // 游戏目录，null 表示未找到
  bool config_found{false};             // 配置文件是否存在
  bool game_dir_found{false};           // gameDir 字段是否存在
  std::string message;                  // 状态描述信息
};

}  // namespace Plugins::InfinityNikki