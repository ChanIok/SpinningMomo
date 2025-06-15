module;

#include <windows.h>

export module Core.ConfigManager;

import std;
import Core.Constants;
import Common.Resolution;
import Common.Types;

namespace Core::Config {

// 使用 Common::Types 中的类型
using RatioPreset = Common::Types::RatioPreset;
using ResolutionPreset = Common::Types::ResolutionPreset;

// 配置结构体定义
export struct HotkeyConfig {
  UINT modifiers = MOD_CONTROL | MOD_ALT;
  UINT key = 'R';
};

export struct WindowConfig {
  std::wstring title;
};

export struct LanguageConfig {
  std::wstring current_language;
};

export struct TaskbarConfig {
  bool auto_hide = false;
  bool lower = true;
};

export struct MenuConfig {
  bool use_floating_window = true;
  std::vector<std::wstring> items_to_show;
  std::vector<std::wstring> aspect_ratio_items;
  std::vector<std::wstring> resolution_items;
};

export struct GameAlbumConfig {
  std::wstring screenshot_path;
};

export struct LetterboxConfig {
  bool enabled = false;
};

// 配置加载结果
export struct ConfigLoadResult {
  bool success = true;
  std::vector<RatioPreset> ratios;
  std::vector<ResolutionPreset> resolutions;
  std::wstring error_details;

  ConfigLoadResult() = default;
  explicit ConfigLoadResult(const std::wstring& error) : success(false), error_details(error) {}
};

// 配置管理器主类
export class ConfigManager {
 public:
  ConfigManager() = default;
  ~ConfigManager() = default;

  // 初始化
  auto Initialize() -> std::expected<void, std::string>;

  // 配置加载
  auto LoadAllConfigs() -> std::expected<void, std::string>;
  auto GetHotkeyConfig() -> std::expected<HotkeyConfig, std::string>;
  auto GetWindowConfig() -> std::expected<WindowConfig, std::string>;
  auto GetLanguageConfig() -> std::expected<LanguageConfig, std::string>;
  auto GetTaskbarConfig() -> std::expected<TaskbarConfig, std::string>;
  auto GetMenuConfig() -> std::expected<MenuConfig, std::string>;
  auto GetGameAlbumConfig() -> std::expected<GameAlbumConfig, std::string>;
  auto GetLetterboxConfig() -> std::expected<LetterboxConfig, std::string>;

  // 配置保存
  auto SetHotkeyConfig(const HotkeyConfig& config) -> std::expected<void, std::string>;
  auto SetWindowConfig(const WindowConfig& config) -> std::expected<void, std::string>;
  auto SetLanguageConfig(const LanguageConfig& config) -> std::expected<void, std::string>;
  auto SetTaskbarConfig(const TaskbarConfig& config) -> std::expected<void, std::string>;
  auto SetMenuConfig(const MenuConfig& config) -> std::expected<void, std::string>;
  auto SetGameAlbumConfig(const GameAlbumConfig& config) -> std::expected<void, std::string>;
  auto SetLetterboxConfig(const LetterboxConfig& config) -> std::expected<void, std::string>;

  // 获取宽高比和分辨率列表
  auto GetAspectRatios(const Constants::LocalizedStrings& strings) -> ConfigLoadResult;
  auto GetResolutionPresets(const Constants::LocalizedStrings& strings) -> ConfigLoadResult;

  // 获取配置文件路径
  auto GetConfigPath() const -> const std::wstring&;

 private:
  // 辅助方法
  auto CreateDefaultConfig() -> std::expected<void, std::string>;
  auto ReadConfigString(const std::wstring& section, const std::wstring& key,
                        const std::wstring& default_value = L"")
      -> std::expected<std::wstring, std::string>;
  auto WriteConfigString(const std::wstring& section, const std::wstring& key,
                         const std::wstring& value) -> std::expected<void, std::string>;
  auto ReadConfigInt(const std::wstring& section, const std::wstring& key, int default_value = 0)
      -> std::expected<int, std::string>;
  auto WriteConfigInt(const std::wstring& section, const std::wstring& key, int value)
      -> std::expected<void, std::string>;

  // 解析方法
  auto ParseStringList(const std::wstring& str, wchar_t delimiter = L',')
      -> std::vector<std::wstring>;
  auto SerializeStringList(const std::vector<std::wstring>& list, wchar_t delimiter = L',')
      -> std::wstring;

  // 默认预设获取
  auto GetDefaultRatioPresets() -> std::vector<RatioPreset>;
  auto GetDefaultResolutionPresets() -> std::vector<ResolutionPreset>;

  // 自定义配置解析
  auto AddCustomRatio(const std::wstring& ratio, std::vector<RatioPreset>& ratios) -> bool;
  auto AddCustomResolution(const std::wstring& resolution,
                           std::vector<ResolutionPreset>& resolutions) -> bool;

  // 成员变量
  std::wstring m_config_path;

  // 缓存的配置
  HotkeyConfig m_hotkey_config;
  WindowConfig m_window_config;
  LanguageConfig m_language_config;
  TaskbarConfig m_taskbar_config;
  MenuConfig m_menu_config;
  GameAlbumConfig m_game_album_config;
  LetterboxConfig m_letterbox_config;
};

}  // namespace Core::Config