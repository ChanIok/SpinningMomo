module;

module Core.Commands;

import std;
import Core.State;
import Core.Commands.State;
import Features.Settings.State;
import Utils.Logger;
import <windows.h>;

namespace Core::Commands {

auto register_command(CommandRegistry& registry, CommandDescriptor descriptor) -> void {
  const std::string id = descriptor.id;

  if (registry.descriptors.contains(id)) {
    Logger().warn("Command already registered: {}", id);
    return;
  }

  registry.descriptors.emplace(id, std::move(descriptor));
  registry.registration_order.push_back(id);

  Logger().debug("Registered command: {}", id);
}

auto invoke_command(CommandRegistry& registry, const std::string& id) -> bool {
  auto it = registry.descriptors.find(id);
  if (it == registry.descriptors.end()) {
    Logger().warn("Command not found: {}", id);
    return false;
  }

  if (!it->second.action) {
    Logger().warn("Command has no action: {}", id);
    return false;
  }

  try {
    it->second.action();
    Logger().debug("Invoked command: {}", id);
    return true;
  } catch (const std::exception& e) {
    Logger().error("Failed to invoke command {}: {}", id, e.what());
    return false;
  }
}

auto get_command(const CommandRegistry& registry, const std::string& id)
    -> const CommandDescriptor* {
  auto it = registry.descriptors.find(id);
  if (it == registry.descriptors.end()) {
    return nullptr;
  }
  return &it->second;
}

auto get_all_commands(const CommandRegistry& registry) -> std::vector<CommandDescriptor> {
  std::vector<CommandDescriptor> result;
  result.reserve(registry.registration_order.size());

  for (const auto& id : registry.registration_order) {
    auto it = registry.descriptors.find(id);
    if (it != registry.descriptors.end()) {
      result.push_back(it->second);
    }
  }

  return result;
}

// 从 settings 获取热键配置（根据 settings_path）
auto get_hotkey_from_settings(const Core::State::AppState& state, const HotkeyBinding& binding)
    -> std::pair<UINT, UINT> {
  const auto& settings = state.settings->raw;

  if (binding.settings_path == "app.hotkey.floating_window") {
    auto result = std::pair{settings.app.hotkey.floating_window.modifiers,
                            settings.app.hotkey.floating_window.key};
    Logger().debug("Hotkey config for '{}': modifiers={}, key={}", binding.settings_path,
                   result.first, result.second);
    return result;
  } else if (binding.settings_path == "app.hotkey.screenshot") {
    auto result =
        std::pair{settings.app.hotkey.screenshot.modifiers, settings.app.hotkey.screenshot.key};
    Logger().debug("Hotkey config for '{}': modifiers={}, key={}", binding.settings_path,
                   result.first, result.second);
    return result;
  } else if (binding.settings_path == "app.hotkey.recording") {
    auto result =
        std::pair{settings.app.hotkey.recording.modifiers, settings.app.hotkey.recording.key};
    Logger().debug("Hotkey config for '{}': modifiers={}, key={}", binding.settings_path,
                   result.first, result.second);
    return result;
  }

  // 如果没有配置，使用默认值
  Logger().debug("Using default hotkey for '{}': modifiers={}, key={}", binding.settings_path,
                 binding.modifiers, binding.key);
  return {binding.modifiers, binding.key};
}

auto register_all_hotkeys(Core::State::AppState& state, HWND hwnd) -> void {
  Logger().info("=== Starting hotkey registration ===");

  if (!hwnd) {
    Logger().error("Cannot register hotkeys: HWND is null");
    return;
  }

  Logger().debug("HWND for hotkey registration: {}", reinterpret_cast<void*>(hwnd));

  auto& cmd_state = *state.commands;
  cmd_state.hotkey_to_command.clear();
  cmd_state.next_hotkey_id = 1;

  Logger().info("Total commands in registry: {}", cmd_state.registry.descriptors.size());

  for (const auto& [id, descriptor] : cmd_state.registry.descriptors) {
    if (!descriptor.hotkey) {
      continue;
    }

    const auto& binding = *descriptor.hotkey;
    Logger().debug("Processing hotkey for command '{}', settings_path='{}'", id,
                   binding.settings_path);

    auto [modifiers, key] = get_hotkey_from_settings(state, binding);

    if (key == 0) {
      Logger().warn("Hotkey key is 0 for command '{}', skipping registration", id);
      continue;
    }

    int hotkey_id = cmd_state.next_hotkey_id++;

    if (::RegisterHotKey(hwnd, hotkey_id, modifiers, key)) {
      cmd_state.hotkey_to_command[hotkey_id] = id;
      Logger().info("Successfully registered hotkey {} for command '{}' (modifiers={}, key={})",
                    hotkey_id, id, modifiers, key);
    } else {
      DWORD error = ::GetLastError();
      Logger().error(
          "Failed to register hotkey for command '{}' (modifiers={}, key={}), error code: {}", id,
          modifiers, key, error);
    }
  }

  Logger().info("=== Hotkey registration complete: {} hotkeys registered ===",
                cmd_state.hotkey_to_command.size());
}

auto unregister_all_hotkeys(Core::State::AppState& state, HWND hwnd) -> void {
  if (!hwnd) {
    return;
  }

  auto& cmd_state = *state.commands;

  for (const auto& [hotkey_id, _] : cmd_state.hotkey_to_command) {
    ::UnregisterHotKey(hwnd, hotkey_id);
  }

  Logger().info("Unregistered {} hotkeys", cmd_state.hotkey_to_command.size());
  cmd_state.hotkey_to_command.clear();
}

auto handle_hotkey(Core::State::AppState& state, int hotkey_id) -> std::optional<std::string> {
  Logger().debug("Received hotkey event, hotkey_id={}", hotkey_id);

  auto& cmd_state = *state.commands;

  auto it = cmd_state.hotkey_to_command.find(hotkey_id);
  if (it != cmd_state.hotkey_to_command.end()) {
    const auto& command_id = it->second;
    Logger().info("Hotkey {} mapped to command '{}', invoking...", hotkey_id, command_id);
    invoke_command(cmd_state.registry, command_id);
    return command_id;
  }

  Logger().warn("Hotkey {} not found in hotkey_to_command map (map size: {})", hotkey_id,
                cmd_state.hotkey_to_command.size());
  return std::nullopt;
}

}  // namespace Core::Commands
