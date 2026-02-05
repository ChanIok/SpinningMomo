module;

export module Core.Commands.State;

import std;
import Core.Commands;

namespace Core::Commands::State {

export struct CommandState {
  CommandRegistry registry;

  // 热键运行时状态
  std::unordered_map<int, std::string> hotkey_to_command;  // hotkey_id -> command_id
  int next_hotkey_id = 1;                                  // 下一个可用的热键ID
};

}  // namespace Core::Commands::State
