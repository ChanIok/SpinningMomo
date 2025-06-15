module;

#include <windows.h>  //必须放在最前面
#include <shellapi.h>
#include <shlwapi.h>

module Core.ConfigManager;

import std;
import Core.Constants;
import Common.Resolution;
import Common.Types;

namespace Core::Config {

// 使用 Common::Types 中的类型
using RatioPreset = Common::Types::RatioPreset;
using ResolutionPreset = Common::Types::ResolutionPreset;

// 字符串转换：UTF-8转换
namespace {
[[nodiscard]] auto to_utf8(const std::wstring& wide_str) noexcept -> std::string {
  if (wide_str.empty()) [[likely]]
    return {};

  const auto size_needed =
      WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), static_cast<int>(wide_str.size()), nullptr,
                          0, nullptr, nullptr);

  if (size_needed <= 0) [[unlikely]]
    return {};

  std::string result(size_needed, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), static_cast<int>(wide_str.size()),
                      result.data(), size_needed, nullptr, nullptr);

  return result;
}
}  // namespace

// 初始化配置管理器
auto ConfigManager::Initialize() -> std::expected<void, std::string> {
  try {
    // 获取程序所在目录
    wchar_t exe_path[MAX_PATH] = {0};
    if (GetModuleFileNameW(NULL, exe_path, MAX_PATH) == 0) {
      m_config_path = L".\\";
    } else {
      PathRemoveFileSpecW(exe_path);
      m_config_path = std::wstring(exe_path) + L"\\";
    }

    m_config_path += Constants::CONFIG_FILE;

    // 检查配置文件是否存在，如果不存在则创建默认配置
    if (GetFileAttributes(m_config_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
      auto result = CreateDefaultConfig();
      if (!result) {
        return std::unexpected(result.error());
      }
    }

    return {};
  } catch (const std::exception& e) {
    return std::unexpected("Failed to initialize config manager: " + std::string(e.what()));
  }
}

// 创建默认配置
auto ConfigManager::CreateDefaultConfig() -> std::expected<void, std::string> {
  // 写入默认配置值
  auto write_default = [this](const std::wstring& section, const std::wstring& key,
                              const std::wstring& value) {
    return WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(),
                                      m_config_path.c_str()) != 0;
  };

  if (!write_default(Constants::WINDOW_SECTION, Constants::WINDOW_TITLE, L"") ||
      !write_default(Constants::HOTKEY_SECTION, Constants::HOTKEY_MODIFIERS, L"3") ||
      !write_default(Constants::HOTKEY_SECTION, Constants::HOTKEY_KEY, L"82") ||
      !write_default(Constants::MENU_SECTION, Constants::MENU_FLOATING, L"1") ||
      !write_default(Constants::SCREENSHOT_SECTION, Constants::SCREENSHOT_PATH, L"") ||
      !write_default(Constants::MENU_SECTION, Constants::MENU_ITEMS,
                     L"CaptureWindow,OpenScreenshot,PreviewWindow,OverlayWindow,LetterboxWindow,"
                     L"Reset,Close,Exit") ||
      !write_default(Constants::MENU_SECTION, Constants::ASPECT_RATIO_ITEMS,
                     L"32:9,21:9,16:9,3:2,1:1,3:4,2:3,9:16") ||
      !write_default(Constants::MENU_SECTION, Constants::RESOLUTION_ITEMS,
                     L"Default,1080P,2K,4K,6K,8K,12K") ||
      !write_default(Constants::LETTERBOX_SECTION, Constants::LETTERBOX_ENABLED, L"0")) {
    return std::unexpected("Failed to create default configuration file");
  }

  return {};
}

// 加载所有配置
auto ConfigManager::LoadAllConfigs() -> std::expected<void, std::string> {
  std::vector<std::string> errors;

  // 尝试加载每个配置，收集错误
  if (auto result = GetHotkeyConfig(); !result) {
    errors.push_back("Hotkey: " + result.error());
  } else {
    m_hotkey_config = result.value();
  }

  if (auto result = GetWindowConfig(); !result) {
    errors.push_back("Window: " + result.error());
  } else {
    m_window_config = result.value();
  }

  if (auto result = GetLanguageConfig(); !result) {
    errors.push_back("Language: " + result.error());
  } else {
    m_language_config = result.value();
  }

  if (auto result = GetTaskbarConfig(); !result) {
    errors.push_back("Taskbar: " + result.error());
  } else {
    m_taskbar_config = result.value();
  }

  if (auto result = GetMenuConfig(); !result) {
    errors.push_back("Menu: " + result.error());
  } else {
    m_menu_config = result.value();
  }

  if (auto result = GetGameAlbumConfig(); !result) {
    errors.push_back("GameAlbum: " + result.error());
  } else {
    m_game_album_config = result.value();
  }

  if (auto result = GetLetterboxConfig(); !result) {
    errors.push_back("Letterbox: " + result.error());
  } else {
    m_letterbox_config = result.value();
  }

  if (!errors.empty()) {
    std::string combined_error = "Failed to load configurations: ";
    for (size_t i = 0; i < errors.size(); ++i) {
      combined_error += errors[i];
      if (i < errors.size() - 1) combined_error += "; ";
    }
    return std::unexpected(combined_error);
  }

  return {};
}

// 获取热键配置
auto ConfigManager::GetHotkeyConfig() -> std::expected<HotkeyConfig, std::string> {
  HotkeyConfig config;

  if (auto modifiers = ReadConfigInt(Constants::HOTKEY_SECTION, Constants::HOTKEY_MODIFIERS, 3)) {
    config.modifiers = static_cast<UINT>(modifiers.value());
  } else {
    return std::unexpected("Failed to read hotkey modifiers: " + modifiers.error());
  }

  if (auto key = ReadConfigInt(Constants::HOTKEY_SECTION, Constants::HOTKEY_KEY, 82)) {
    config.key = static_cast<UINT>(key.value());
  } else {
    return std::unexpected("Failed to read hotkey key: " + key.error());
  }

  return config;
}

// 设置热键配置
auto ConfigManager::SetHotkeyConfig(const HotkeyConfig& config)
    -> std::expected<void, std::string> {
  if (auto result =
          WriteConfigInt(Constants::HOTKEY_SECTION, Constants::HOTKEY_MODIFIERS, config.modifiers);
      !result) {
    return std::unexpected("Failed to write hotkey modifiers: " + result.error());
  }

  if (auto result = WriteConfigInt(Constants::HOTKEY_SECTION, Constants::HOTKEY_KEY, config.key);
      !result) {
    return std::unexpected("Failed to write hotkey key: " + result.error());
  }

  m_hotkey_config = config;
  return {};
}

// 获取窗口配置
auto ConfigManager::GetWindowConfig() -> std::expected<WindowConfig, std::string> {
  WindowConfig config;

  if (auto title = ReadConfigString(Constants::WINDOW_SECTION, Constants::WINDOW_TITLE, L"")) {
    config.title = title.value();
  } else {
    return std::unexpected("Failed to read window title: " + title.error());
  }

  return config;
}

// 设置窗口配置
auto ConfigManager::SetWindowConfig(const WindowConfig& config)
    -> std::expected<void, std::string> {
  std::wstring title_to_write = config.title.empty() ? L"" : L"\"" + config.title + L"\"";

  if (auto result =
          WriteConfigString(Constants::WINDOW_SECTION, Constants::WINDOW_TITLE, title_to_write);
      !result) {
    return std::unexpected("Failed to write window title: " + result.error());
  }

  m_window_config = config;
  return {};
}

// 获取语言配置
auto ConfigManager::GetLanguageConfig() -> std::expected<LanguageConfig, std::string> {
  LanguageConfig config;

  if (auto lang = ReadConfigString(Constants::LANG_SECTION, Constants::LANG_CURRENT, L"")) {
    if (lang->empty()) {
      // 如果配置为空，根据系统语言设置默认值
      LANGID lang_id = GetUserDefaultUILanguage();
      WORD primary_lang_id = PRIMARYLANGID(lang_id);
      config.current_language =
          (primary_lang_id == LANG_CHINESE) ? Constants::LANG_ZH_CN : Constants::LANG_EN_US;

      // 保存默认语言设置
      if (auto save_result = SetLanguageConfig(config); !save_result) {
        return std::unexpected("Failed to save default language: " + save_result.error());
      }
    } else {
      config.current_language = lang.value();
    }
  } else {
    return std::unexpected("Failed to read language config: " + lang.error());
  }

  return config;
}

// 设置语言配置
auto ConfigManager::SetLanguageConfig(const LanguageConfig& config)
    -> std::expected<void, std::string> {
  if (auto result = WriteConfigString(Constants::LANG_SECTION, Constants::LANG_CURRENT,
                                      config.current_language);
      !result) {
    return std::unexpected("Failed to write language config: " + result.error());
  }

  m_language_config = config;
  return {};
}

// 获取任务栏配置
auto ConfigManager::GetTaskbarConfig() -> std::expected<TaskbarConfig, std::string> {
  TaskbarConfig config;

  if (auto auto_hide = ReadConfigInt(Constants::TASKBAR_SECTION, Constants::TASKBAR_AUTOHIDE, 0)) {
    config.auto_hide = (auto_hide.value() != 0);
  } else {
    return std::unexpected("Failed to read taskbar auto hide: " + auto_hide.error());
  }

  if (auto lower = ReadConfigInt(Constants::TASKBAR_SECTION, Constants::TASKBAR_LOWER, 1)) {
    config.lower = (lower.value() != 0);
  } else {
    return std::unexpected("Failed to read taskbar lower: " + lower.error());
  }

  return config;
}

// 设置任务栏配置
auto ConfigManager::SetTaskbarConfig(const TaskbarConfig& config)
    -> std::expected<void, std::string> {
  if (auto result = WriteConfigInt(Constants::TASKBAR_SECTION, Constants::TASKBAR_AUTOHIDE,
                                   config.auto_hide ? 1 : 0);
      !result) {
    return std::unexpected("Failed to write taskbar auto hide: " + result.error());
  }

  if (auto result = WriteConfigInt(Constants::TASKBAR_SECTION, Constants::TASKBAR_LOWER,
                                   config.lower ? 1 : 0);
      !result) {
    return std::unexpected("Failed to write taskbar lower: " + result.error());
  }

  m_taskbar_config = config;
  return {};
}

// 获取菜单配置
auto ConfigManager::GetMenuConfig() -> std::expected<MenuConfig, std::string> {
  MenuConfig config;

  // 读取浮动窗口设置
  if (auto floating = ReadConfigInt(Constants::MENU_SECTION, Constants::MENU_FLOATING, 1)) {
    config.use_floating_window = (floating.value() != 0);
  } else {
    return std::unexpected("Failed to read menu floating: " + floating.error());
  }

  // 读取菜单项
  if (auto items_str = ReadConfigString(Constants::MENU_SECTION, Constants::MENU_ITEMS, L"")) {
    config.items_to_show = ParseStringList(items_str.value());
  } else {
    return std::unexpected("Failed to read menu items: " + items_str.error());
  }

  // 读取宽高比项
  if (auto ratio_str =
          ReadConfigString(Constants::MENU_SECTION, Constants::ASPECT_RATIO_ITEMS, L"")) {
    config.aspect_ratio_items = ParseStringList(ratio_str.value());
  } else {
    return std::unexpected("Failed to read aspect ratio items: " + ratio_str.error());
  }

  // 读取分辨率项
  if (auto resolution_str =
          ReadConfigString(Constants::MENU_SECTION, Constants::RESOLUTION_ITEMS, L"")) {
    config.resolution_items = ParseStringList(resolution_str.value());
  } else {
    return std::unexpected("Failed to read resolution items: " + resolution_str.error());
  }

  return config;
}

// 设置菜单配置
auto ConfigManager::SetMenuConfig(const MenuConfig& config) -> std::expected<void, std::string> {
  if (auto result = WriteConfigInt(Constants::MENU_SECTION, Constants::MENU_FLOATING,
                                   config.use_floating_window ? 1 : 0);
      !result) {
    return std::unexpected("Failed to write menu floating: " + result.error());
  }

  m_menu_config = config;
  return {};
}

// 获取游戏相册配置
auto ConfigManager::GetGameAlbumConfig() -> std::expected<GameAlbumConfig, std::string> {
  GameAlbumConfig config;

  if (auto path =
          ReadConfigString(Constants::SCREENSHOT_SECTION, Constants::SCREENSHOT_PATH, L"")) {
    config.screenshot_path = path.value();
  } else {
    return std::unexpected("Failed to read screenshot path: " + path.error());
  }

  return config;
}

// 设置游戏相册配置
auto ConfigManager::SetGameAlbumConfig(const GameAlbumConfig& config)
    -> std::expected<void, std::string> {
  if (auto result = WriteConfigString(Constants::SCREENSHOT_SECTION, Constants::SCREENSHOT_PATH,
                                      config.screenshot_path);
      !result) {
    return std::unexpected("Failed to write screenshot path: " + result.error());
  }

  m_game_album_config = config;
  return {};
}

// 获取黑边配置
auto ConfigManager::GetLetterboxConfig() -> std::expected<LetterboxConfig, std::string> {
  LetterboxConfig config;

  if (auto enabled = ReadConfigInt(Constants::LETTERBOX_SECTION, Constants::LETTERBOX_ENABLED, 0)) {
    config.enabled = (enabled.value() != 0);
  } else {
    return std::unexpected("Failed to read letterbox enabled: " + enabled.error());
  }

  return config;
}

// 设置黑边配置
auto ConfigManager::SetLetterboxConfig(const LetterboxConfig& config)
    -> std::expected<void, std::string> {
  if (auto result = WriteConfigInt(Constants::LETTERBOX_SECTION, Constants::LETTERBOX_ENABLED,
                                   config.enabled ? 1 : 0);
      !result) {
    return std::unexpected("Failed to write letterbox enabled: " + result.error());
  }

  m_letterbox_config = config;
  return {};
}

// 获取配置文件路径
auto ConfigManager::GetConfigPath() const -> const std::wstring& { return m_config_path; }

// 读取配置字符串
auto ConfigManager::ReadConfigString(const std::wstring& section, const std::wstring& key,
                                     const std::wstring& default_value)
    -> std::expected<std::wstring, std::string> {
  wchar_t buffer[1024];
  DWORD result = GetPrivateProfileStringW(section.c_str(), key.c_str(), default_value.c_str(),
                                          buffer, _countof(buffer), m_config_path.c_str());

  if (result == 0 && GetLastError() != ERROR_SUCCESS) {
    return std::unexpected(
        std::format("Failed to read config string for {}.{}", to_utf8(section), to_utf8(key)));
  }

  return std::wstring(buffer);
}

// 写入配置字符串
auto ConfigManager::WriteConfigString(const std::wstring& section, const std::wstring& key,
                                      const std::wstring& value)
    -> std::expected<void, std::string> {
  if (!WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(),
                                  m_config_path.c_str())) {
    return std::unexpected(
        std::format("Failed to write config string for {}.{}", to_utf8(section), to_utf8(key)));
  }

  return {};
}

// 读取配置整数
auto ConfigManager::ReadConfigInt(const std::wstring& section, const std::wstring& key,
                                  int default_value) -> std::expected<int, std::string> {
  wchar_t buffer[32];
  if (GetPrivateProfileStringW(section.c_str(), key.c_str(), L"", buffer, _countof(buffer),
                               m_config_path.c_str()) > 0) {
    try {
      return std::stoi(buffer);
    } catch (const std::exception& e) {
      return std::unexpected("Failed to parse integer value: " + std::string(e.what()));
    }
  }

  return default_value;
}

// 写入配置整数
auto ConfigManager::WriteConfigInt(const std::wstring& section, const std::wstring& key, int value)
    -> std::expected<void, std::string> {
  return WriteConfigString(section, key, std::format(L"{}", value));
}

// 解析字符串列表
auto ConfigManager::ParseStringList(const std::wstring& str, wchar_t delimiter)
    -> std::vector<std::wstring> {
  std::vector<std::wstring> result;

  if (str.empty()) {
    return result;
  }

  size_t start = 0, end = 0;
  while ((end = str.find(delimiter, start)) != std::wstring::npos) {
    std::wstring item = str.substr(start, end - start);
    if (!item.empty()) {
      result.push_back(item);
    }
    start = end + 1;
  }

  if (start < str.length()) {
    std::wstring item = str.substr(start);
    if (!item.empty()) {
      result.push_back(item);
    }
  }

  return result;
}

// 序列化字符串列表
auto ConfigManager::SerializeStringList(const std::vector<std::wstring>& list, wchar_t delimiter)
    -> std::wstring {
  if (list.empty()) {
    return L"";
  }

  std::wstring result = list[0];
  for (size_t i = 1; i < list.size(); ++i) {
    result += delimiter + list[i];
  }

  return result;
}

// 获取宽高比列表
auto ConfigManager::GetAspectRatios(const Constants::LocalizedStrings& strings)
    -> ConfigLoadResult {
  ConfigLoadResult result;

  // 1. 获取默认预设
  result.ratios = GetDefaultRatioPresets();

  // 2. 如果配置中没有自定义项，直接返回默认预设
  if (m_menu_config.aspect_ratio_items.empty()) {
    return result;
  }

  // 3. 处理配置中的自定义项
  std::vector<RatioPreset> configured_ratios;
  bool has_error = false;
  std::wstring error_details;

  for (const auto& item : m_menu_config.aspect_ratio_items) {
    // 检查是否是预定义项
    bool found = false;
    for (const auto& preset : result.ratios) {
      if (item == preset.name) {
        configured_ratios.push_back(preset);
        found = true;
        break;
      }
    }

    // 如果不是预定义项，尝试解析为自定义比例
    if (!found) {
      if (!AddCustomRatio(item, configured_ratios)) {
        has_error = true;
        error_details += item + L", ";
      }
    }
  }

  // 如果有有效的配置项，用它们替换默认预设
  if (!configured_ratios.empty()) {
    result.ratios = configured_ratios;
  }

  // 如果有错误，设置错误信息
  if (has_error) {
    if (error_details.length() >= 2) {
      error_details = error_details.substr(0, error_details.length() - 2);
    }
    result.success = false;
    result.error_details =
        strings.CONFIG_FORMAT_ERROR + L" " + error_details + L"\n" + strings.RATIO_FORMAT_EXAMPLE;
  }

  return result;
}

// 获取分辨率预设列表
auto ConfigManager::GetResolutionPresets(const Constants::LocalizedStrings& strings)
    -> ConfigLoadResult {
  ConfigLoadResult result;

  // 1. 获取默认预设
  result.resolutions = GetDefaultResolutionPresets();

  // 2. 如果配置中没有自定义项，直接返回默认预设
  if (m_menu_config.resolution_items.empty()) {
    return result;
  }

  // 3. 处理配置中的自定义项
  std::vector<ResolutionPreset> configured_resolutions;
  bool has_error = false;
  std::wstring error_details;

  for (const auto& item : m_menu_config.resolution_items) {
    // 检查是否是预定义项
    bool found = false;
    for (const auto& preset : result.resolutions) {
      if (item == preset.name) {
        configured_resolutions.push_back(preset);
        found = true;
        break;
      }
    }

    // 如果不是预定义项，尝试解析为自定义分辨率
    if (!found) {
      if (!AddCustomResolution(item, configured_resolutions)) {
        has_error = true;
        error_details += item + L", ";
      }
    }
  }

  // 如果有有效的配置项，用它们替换默认预设
  if (!configured_resolutions.empty()) {
    result.resolutions = configured_resolutions;
  }

  // 如果有错误，设置错误信息
  if (has_error) {
    if (error_details.length() >= 2) {
      error_details = error_details.substr(0, error_details.length() - 2);
    }
    result.success = false;
    result.error_details = strings.CONFIG_FORMAT_ERROR + L" " + error_details + L"\n" +
                           strings.RESOLUTION_FORMAT_EXAMPLE;
  }

  return result;
}

// 获取默认的宽高比预设
auto ConfigManager::GetDefaultRatioPresets() -> std::vector<RatioPreset> {
  return {
      RatioPreset(L"32:9", 32.0 / 9.0),  // 超宽屏
      RatioPreset(L"21:9", 21.0 / 9.0),  // 宽屏
      RatioPreset(L"16:9", 16.0 / 9.0),  // 标准宽屏
      RatioPreset(L"3:2", 3.0 / 2.0),    // 传统显示器
      RatioPreset(L"1:1", 1.0),          // 正方形
      RatioPreset(L"3:4", 3.0 / 4.0),    // 竖屏
      RatioPreset(L"2:3", 2.0 / 3.0),    // 竖屏
      RatioPreset(L"9:16", 9.0 / 16.0)   // 竖屏宽屏
  };
}

// 获取默认的分辨率预设
auto ConfigManager::GetDefaultResolutionPresets() -> std::vector<ResolutionPreset> {
  return {
      ResolutionPreset(L"Default", 0, 0),      // 默认选项，使用屏幕尺寸计算
      ResolutionPreset(L"1080P", 1920, 1080),  // 2.1M pixels
      ResolutionPreset(L"2K", 2560, 1440),     // 3.7M pixels
      ResolutionPreset(L"4K", 3840, 2160),     // 8.3M pixels
      ResolutionPreset(L"6K", 5760, 3240),     // 18.7M pixels
      ResolutionPreset(L"8K", 7680, 4320),     // 33.2M pixels
      ResolutionPreset(L"12K", 11520, 6480)    // 74.6M pixels
  };
}

// 添加自定义宽高比
auto ConfigManager::AddCustomRatio(const std::wstring& ratio, std::vector<RatioPreset>& ratios)
    -> bool {
  size_t colon_pos = ratio.find(L":");
  if (colon_pos == std::wstring::npos) return false;

  try {
    double width = std::stod(ratio.substr(0, colon_pos));
    double height = std::stod(ratio.substr(colon_pos + 1));
    if (height <= 0) return false;

    ratios.emplace_back(ratio, width / height);
    return true;
  } catch (...) {
    return false;
  }
}

// 添加自定义分辨率
auto ConfigManager::AddCustomResolution(const std::wstring& resolution,
                                        std::vector<ResolutionPreset>& resolutions) -> bool {
  try {
    // 处理常见分辨率标识符
    if (resolution == L"480P") {
      resolutions.emplace_back(resolution, 720, 480);
      return true;
    } else if (resolution == L"720P") {
      resolutions.emplace_back(resolution, 1280, 720);
      return true;
    } else if (resolution == L"1080P") {
      resolutions.emplace_back(resolution, 1920, 1080);
      return true;
    } else if (resolution == L"2K") {
      resolutions.emplace_back(resolution, 2560, 1440);
      return true;
    } else if (resolution == L"5K") {
      resolutions.emplace_back(resolution, 5120, 2880);
      return true;
    } else if (resolution == L"10K") {
      resolutions.emplace_back(resolution, 10240, 4320);
      return true;
    } else if (resolution == L"16K") {
      resolutions.emplace_back(resolution, 15360, 8640);
      return true;
    }

    // 处理自定义分辨率格式 (例如 1920x1080)
    size_t x_pos = resolution.find(L"x");
    if (x_pos == std::wstring::npos) return false;

    int width = std::stoi(resolution.substr(0, x_pos));
    int height = std::stoi(resolution.substr(x_pos + 1));

    if (width <= 0 || height <= 0) return false;

    resolutions.emplace_back(resolution, width, height);
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace Core::Config