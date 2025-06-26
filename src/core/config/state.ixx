module;

export module Core.Config.State;

import std;
import Types.Presets;
import Vendor.Windows;

export namespace Core::Config::State {

struct HotkeyConfig {
  Vendor::Windows::UINT modifiers = Vendor::Windows::MOD_CONTROL_t | Vendor::Windows::MOD_ALT_t;
  Vendor::Windows::UINT key = 'R';
};

struct WindowConfig {
  std::wstring title;
};

struct LanguageConfig {
  std::wstring current_language;
};

struct TaskbarConfig {
  bool auto_hide = false;
  bool lower = true;
};

struct MenuConfig {
  bool use_floating_window = true;
  std::vector<std::wstring> items_to_show;
  std::vector<std::wstring> aspect_ratio_items;
  std::vector<std::wstring> resolution_items;
};

struct GameAlbumConfig {
  std::wstring screenshot_path;
};

struct LetterboxConfig {
  bool enabled = false;
};

struct AppConfig {
  HotkeyConfig hotkey;
  WindowConfig window;
  LanguageConfig language;
  TaskbarConfig taskbar;
  MenuConfig menu;
  GameAlbumConfig game_album;
  LetterboxConfig letterbox;

  // 元数据
  std::wstring config_file_path;
};

struct RatioLoadResult {
  bool success = true;
  std::vector<Types::Presets::RatioPreset> ratios;
  std::wstring error_details;

  RatioLoadResult() = default;
  explicit RatioLoadResult(const std::wstring& error) : success(false), error_details(error) {}
};

struct ResolutionLoadResult {
  bool success = true;
  std::vector<Types::Presets::ResolutionPreset> resolutions;
  std::wstring error_details;

  ResolutionLoadResult() = default;
  explicit ResolutionLoadResult(const std::wstring& error) : success(false), error_details(error) {}
};

}  // namespace Core::Config::State
