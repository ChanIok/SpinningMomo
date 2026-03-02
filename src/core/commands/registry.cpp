module;

module Core.Commands;

import std;
import Core.State;
import Core.Commands.State;
import Features.Settings.State;
import Utils.Logger;
import <windows.h>;

namespace Core::Commands {

static Core::Commands::State::CommandState* g_mouse_hotkey_state = nullptr;

auto is_mouse_side_key(UINT key) -> bool { return key == VK_XBUTTON1 || key == VK_XBUTTON2; }

auto make_mouse_combo(UINT modifiers, UINT key) -> std::uint32_t {
  constexpr UINT kSupportedModifiers = MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN;
  auto masked_modifiers = modifiers & kSupportedModifiers;
  return (static_cast<std::uint32_t>(masked_modifiers & 0xFFFFu) << 16u) |
         static_cast<std::uint32_t>(key & 0xFFFFu);
}

auto get_current_hotkey_modifiers() -> UINT {
  UINT modifiers = 0;
  if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) {
    modifiers |= MOD_ALT;
  }
  if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) {
    modifiers |= MOD_CONTROL;
  }
  if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) {
    modifiers |= MOD_SHIFT;
  }
  if ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0) {
    modifiers |= MOD_WIN;
  }
  return modifiers;
}

LRESULT CALLBACK mouse_hotkey_proc(int code, WPARAM wParam, LPARAM lParam) {
  if (code == HC_ACTION && g_mouse_hotkey_state && wParam == WM_XBUTTONDOWN) {
    auto* mouse_info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
    if (mouse_info) {
      UINT key = 0;
      auto button = HIWORD(mouse_info->mouseData);
      if (button == XBUTTON1) {
        key = VK_XBUTTON1;
      } else if (button == XBUTTON2) {
        key = VK_XBUTTON2;
      }

      if (key != 0) {
        auto combo = make_mouse_combo(get_current_hotkey_modifiers(), key);
        auto it = g_mouse_hotkey_state->mouse_combo_to_hotkey_id.find(combo);
        if (it != g_mouse_hotkey_state->mouse_combo_to_hotkey_id.end()) {
          auto target_hwnd = g_mouse_hotkey_state->mouse_hotkey_target_hwnd;
          if (target_hwnd && IsWindow(target_hwnd)) {
            PostMessageW(target_hwnd, WM_HOTKEY, static_cast<WPARAM>(it->second), 0);
          }
        }
      }
    }
  }

  return CallNextHookEx(nullptr, code, wParam, lParam);
}

auto install_mouse_hotkey_hook(Core::Commands::State::CommandState& cmd_state, HWND hwnd) -> bool {
  if (cmd_state.mouse_hotkey_hook) {
    cmd_state.mouse_hotkey_target_hwnd = hwnd;
    g_mouse_hotkey_state = &cmd_state;
    return true;
  }

  auto hook = SetWindowsHookExW(WH_MOUSE_LL, mouse_hotkey_proc, GetModuleHandleW(nullptr), 0);
  if (!hook) {
    auto error = GetLastError();
    Logger().error("Failed to install mouse hotkey hook, error code: {}", error);
    return false;
  }

  cmd_state.mouse_hotkey_hook = hook;
  cmd_state.mouse_hotkey_target_hwnd = hwnd;
  g_mouse_hotkey_state = &cmd_state;
  Logger().info("Mouse hotkey hook installed");
  return true;
}

auto uninstall_mouse_hotkey_hook(Core::Commands::State::CommandState& cmd_state) -> void {
  if (cmd_state.mouse_hotkey_hook) {
    UnhookWindowsHookEx(cmd_state.mouse_hotkey_hook);
    cmd_state.mouse_hotkey_hook = nullptr;
    Logger().info("Mouse hotkey hook uninstalled");
  }

  cmd_state.mouse_hotkey_target_hwnd = nullptr;
  cmd_state.mouse_combo_to_hotkey_id.clear();
  if (g_mouse_hotkey_state == &cmd_state) {
    g_mouse_hotkey_state = nullptr;
  }
}

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
  uninstall_mouse_hotkey_hook(cmd_state);
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

    if (is_mouse_side_key(key)) {
      auto combo = make_mouse_combo(modifiers, key);
      if (cmd_state.mouse_combo_to_hotkey_id.contains(combo)) {
        Logger().error("Duplicate mouse hotkey for command '{}' (modifiers={}, key={}), skipping",
                       id, modifiers & (MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN), key);
        continue;
      }

      cmd_state.hotkey_to_command[hotkey_id] = id;
      cmd_state.mouse_combo_to_hotkey_id[combo] = hotkey_id;

      Logger().info("Registered mouse hotkey {} for command '{}' (modifiers={}, key={})", hotkey_id,
                    id, modifiers & (MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN), key);
      continue;
    }

    if (::RegisterHotKey(hwnd, hotkey_id, modifiers, key)) {
      cmd_state.hotkey_to_command[hotkey_id] = id;
      Logger().info("Successfully registered hotkey {} for command '{}' (modifiers={}, key={})",
                    hotkey_id, id, modifiers, key);
      continue;
    }

    DWORD error = ::GetLastError();
    Logger().error(
        "Failed to register hotkey for command '{}' (modifiers={}, key={}), error code: {}", id,
        modifiers, key, error);
  }

  if (!cmd_state.mouse_combo_to_hotkey_id.empty() && !install_mouse_hotkey_hook(cmd_state, hwnd)) {
    for (const auto& [_, mouse_hotkey_id] : cmd_state.mouse_combo_to_hotkey_id) {
      cmd_state.hotkey_to_command.erase(mouse_hotkey_id);
    }
    cmd_state.mouse_combo_to_hotkey_id.clear();
    Logger().error("Mouse side-button hotkeys disabled because hook installation failed");
  }

  Logger().info("=== Hotkey registration complete: {} hotkeys registered ===",
                cmd_state.hotkey_to_command.size());
}

auto unregister_all_hotkeys(Core::State::AppState& state, HWND hwnd) -> void {
  auto& cmd_state = *state.commands;
  uninstall_mouse_hotkey_hook(cmd_state);

  if (hwnd) {
    for (const auto& [hotkey_id, _] : cmd_state.hotkey_to_command) {
      ::UnregisterHotKey(hwnd, hotkey_id);
    }
  }

  Logger().info("Unregistered {} hotkeys", cmd_state.hotkey_to_command.size());
  cmd_state.hotkey_to_command.clear();
  cmd_state.next_hotkey_id = 1;
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
