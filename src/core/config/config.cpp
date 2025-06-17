module;

#include <windows.h>
#include <shlwapi.h>
#include <expected>
#include <format>
#include <string>
#include <vector>

module Core.Config.Io;

import Core.Config;
import Core.Constants;
import Common.Types;
import Utils.String;

namespace Core::Config::Io {

// ============================================================================
// 内部辅助函数 (Internal Helper Functions)
// ============================================================================

namespace {

// INI 文件读写辅助函数
auto read_string(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& default_val) -> std::wstring {
    wchar_t buffer[1024];
    GetPrivateProfileStringW(section.c_str(), key.c_str(), default_val.c_str(), buffer, _countof(buffer), path.c_str());
    return buffer;
}

auto write_string(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& value) -> bool {
    return WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(), path.c_str()) != 0;
}

auto read_int(const std::wstring& path, const std::wstring& section, const std::wstring& key, int default_val) -> int {
    return GetPrivateProfileIntW(section.c_str(), key.c_str(), default_val, path.c_str());
}

auto write_int(const std::wstring& path, const std::wstring& section, const std::wstring& key, int value) -> bool {
    return write_string(path, section, key, std::to_wstring(value));
}

// 列表解析与序列化
auto parse_string_list(const std::wstring& str, wchar_t delimiter = L',') -> std::vector<std::wstring> {
    std::vector<std::wstring> result;
    if (str.empty()) return result;
    size_t start = 0, end = 0;
    while ((end = str.find(delimiter, start)) != std::wstring::npos) {
        std::wstring item = str.substr(start, end - start);
        if (!item.empty()) result.push_back(item);
        start = end + 1;
    }
    if (start < str.length()) {
        std::wstring item = str.substr(start);
        if (!item.empty()) result.push_back(item);
    }
    return result;
}

auto serialize_string_list(const std::vector<std::wstring>& list, wchar_t delimiter = L',') -> std::wstring {
    if (list.empty()) return L"";
    std::wstring result = list[0];
    for (size_t i = 1; i < list.size(); ++i) {
        result += delimiter;
        result += list[i];
    }
    return result;
}

// 默认配置创建
auto create_default_config_file(const std::wstring& path) -> std::expected<void, std::string> {
    LANGID lang_id = GetUserDefaultUILanguage();
    WORD primary_lang_id = PRIMARYLANGID(lang_id);
    std::wstring default_lang = (primary_lang_id == LANG_CHINESE) ? Constants::LANG_ZH_CN : Constants::LANG_EN_US;

    bool success =
        write_string(path, Constants::WINDOW_SECTION, Constants::WINDOW_TITLE, L"") &&
        write_int(path, Constants::HOTKEY_SECTION, Constants::HOTKEY_MODIFIERS, 3) &&
        write_int(path, Constants::HOTKEY_SECTION, Constants::HOTKEY_KEY, 82) &&
        write_string(path, Constants::LANG_SECTION, Constants::LANG_CURRENT, default_lang) &&
        write_int(path, Constants::MENU_SECTION, Constants::MENU_FLOATING, 1) &&
        write_string(path, Constants::SCREENSHOT_SECTION, Constants::SCREENSHOT_PATH, L"") &&
        write_string(path, Constants::MENU_SECTION, Constants::MENU_ITEMS, L"CaptureWindow,OpenScreenshot,PreviewWindow,OverlayWindow,LetterboxWindow,Reset,Close,Exit") &&
        write_string(path, Constants::MENU_SECTION, Constants::ASPECT_RATIO_ITEMS, L"32:9,21:9,16:9,3:2,1:1,3:4,2:3,9:16") &&
        write_string(path, Constants::MENU_SECTION, Constants::RESOLUTION_ITEMS, L"Default,1080P,2K,4K,6K,8K,12K") &&
        write_int(path, Constants::TASKBAR_SECTION, Constants::TASKBAR_AUTOHIDE, 0) &&
        write_int(path, Constants::TASKBAR_SECTION, Constants::TASKBAR_LOWER, 1) &&
        write_int(path, Constants::LETTERBOX_SECTION, Constants::LETTERBOX_ENABLED, 0);

    if (!success) {
        return std::unexpected("Failed to create default configuration file.");
    }
    return {};
}

// 从文件加载配置到 AppConfig
auto load_from_file(const std::wstring& path) -> AppConfig {
    AppConfig cfg;
    cfg.config_file_path = path;

    // Hotkey
    cfg.hotkey.modifiers = read_int(path, Constants::HOTKEY_SECTION, Constants::HOTKEY_MODIFIERS, 3);
    cfg.hotkey.key = read_int(path, Constants::HOTKEY_SECTION, Constants::HOTKEY_KEY, 82);

    // Window
    cfg.window.title = read_string(path, Constants::WINDOW_SECTION, Constants::WINDOW_TITLE, L"");

    // Language
    cfg.language.current_language = read_string(path, Constants::LANG_SECTION, Constants::LANG_CURRENT, L"");
    if (cfg.language.current_language.empty()) {
        LANGID lang_id = GetUserDefaultUILanguage();
        WORD primary_lang_id = PRIMARYLANGID(lang_id);
        cfg.language.current_language = (primary_lang_id == LANG_CHINESE) ? Constants::LANG_ZH_CN : Constants::LANG_EN_US;
    }

    // Taskbar
    cfg.taskbar.auto_hide = (read_int(path, Constants::TASKBAR_SECTION, Constants::TASKBAR_AUTOHIDE, 0) != 0);
    cfg.taskbar.lower = (read_int(path, Constants::TASKBAR_SECTION, Constants::TASKBAR_LOWER, 1) != 0);

    // Menu
    cfg.menu.use_floating_window = (read_int(path, Constants::MENU_SECTION, Constants::MENU_FLOATING, 1) != 0);
    cfg.menu.items_to_show = parse_string_list(read_string(path, Constants::MENU_SECTION, Constants::MENU_ITEMS, L""));
    cfg.menu.aspect_ratio_items = parse_string_list(read_string(path, Constants::MENU_SECTION, Constants::ASPECT_RATIO_ITEMS, L""));
    cfg.menu.resolution_items = parse_string_list(read_string(path, Constants::MENU_SECTION, Constants::RESOLUTION_ITEMS, L""));

    // GameAlbum
    cfg.game_album.screenshot_path = read_string(path, Constants::SCREENSHOT_SECTION, Constants::SCREENSHOT_PATH, L"");

    // Letterbox
    cfg.letterbox.enabled = (read_int(path, Constants::LETTERBOX_SECTION, Constants::LETTERBOX_ENABLED, 0) != 0);
    
    return cfg;
}

// 默认预设获取
auto get_default_ratio_presets() -> std::vector<Common::Types::RatioPreset> {
  return {
      {L"32:9", 32.0 / 9.0}, {L"21:9", 21.0 / 9.0}, {L"16:9", 16.0 / 9.0},
      {L"3:2", 3.0 / 2.0}, {L"1:1", 1.0}, {L"3:4", 3.0 / 4.0},
      {L"2:3", 2.0 / 3.0}, {L"9:16", 9.0 / 16.0}
  };
}

auto get_default_resolution_presets() -> std::vector<Common::Types::ResolutionPreset> {
  return {
      {L"Default", 0, 0}, {L"1080P", 1920, 1080}, {L"2K", 2560, 1440},
      {L"4K", 3840, 2160}, {L"6K", 5760, 3240}, {L"8K", 7680, 4320},
      {L"12K", 11520, 6480}
  };
}

// 自定义配置解析
auto add_custom_ratio(const std::wstring& ratio, std::vector<Common::Types::RatioPreset>& ratios) -> bool {
  size_t colon_pos = ratio.find(L':');
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

auto add_custom_resolution(const std::wstring& resolution, std::vector<Common::Types::ResolutionPreset>& resolutions) -> bool {
  try {
    static const std::unordered_map<std::wstring, std::pair<int, int>> presets = {
        {L"480P", {720, 480}}, {L"720P", {1280, 720}}, {L"1080P", {1920, 1080}},
        {L"2K", {2560, 1440}}, {L"5K", {5120, 2880}}, {L"10K", {10240, 4320}},
        {L"16K", {15360, 8640}}
    };
    if (auto it = presets.find(resolution); it != presets.end()) {
        resolutions.emplace_back(resolution, it->second.first, it->second.second);
        return true;
    }
    size_t x_pos = resolution.find(L'x');
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

} // anoymous namespace

// ============================================================================
// 导出的函数实现 (Exported Function Implementations)
// ============================================================================

auto initialize() -> std::expected<AppConfig, std::string> {
    try {
        wchar_t exe_path_buf[MAX_PATH] = {0};
        if (GetModuleFileNameW(nullptr, exe_path_buf, MAX_PATH) == 0) {
            return std::unexpected("Failed to get module file name.");
        }
        
        PathRemoveFileSpecW(exe_path_buf);
        std::wstring config_path = std::wstring(exe_path_buf) + L"\\" + Constants::CONFIG_FILE;

        if (GetFileAttributesW(config_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
            if (auto result = create_default_config_file(config_path); !result) {
                return std::unexpected(result.error());
            }
        }
        
        return load_from_file(config_path);
    } catch (const std::exception& e) {
        return std::unexpected(std::string("Initialization failed: ") + e.what());
    }
}

auto save(const AppConfig& config) -> std::expected<void, std::string> {
    const auto& path = config.config_file_path;
    bool success =
        write_int(path, Constants::HOTKEY_SECTION, Constants::HOTKEY_MODIFIERS, config.hotkey.modifiers) &&
        write_int(path, Constants::HOTKEY_SECTION, Constants::HOTKEY_KEY, config.hotkey.key) &&
        write_string(path, Constants::WINDOW_SECTION, Constants::WINDOW_TITLE, config.window.title) &&
        write_string(path, Constants::LANG_SECTION, Constants::LANG_CURRENT, config.language.current_language) &&
        write_int(path, Constants::TASKBAR_SECTION, Constants::TASKBAR_AUTOHIDE, config.taskbar.auto_hide) &&
        write_int(path, Constants::TASKBAR_SECTION, Constants::TASKBAR_LOWER, config.taskbar.lower) &&
        write_int(path, Constants::MENU_SECTION, Constants::MENU_FLOATING, config.menu.use_floating_window) &&
        write_string(path, Constants::MENU_SECTION, Constants::MENU_ITEMS, serialize_string_list(config.menu.items_to_show)) &&
        write_string(path, Constants::MENU_SECTION, Constants::ASPECT_RATIO_ITEMS, serialize_string_list(config.menu.aspect_ratio_items)) &&
        write_string(path, Constants::MENU_SECTION, Constants::RESOLUTION_ITEMS, serialize_string_list(config.menu.resolution_items)) &&
        write_string(path, Constants::SCREENSHOT_SECTION, Constants::SCREENSHOT_PATH, config.game_album.screenshot_path) &&
        write_int(path, Constants::LETTERBOX_SECTION, Constants::LETTERBOX_ENABLED, config.letterbox.enabled);

    if (!success) {
        return std::unexpected("Failed to save one or more configuration settings.");
    }
    return {};
}


auto get_aspect_ratios(const AppConfig& config, const Constants::LocalizedStrings& strings) -> RatioLoadResult {
    RatioLoadResult result;
    auto default_presets = get_default_ratio_presets();

    if (config.menu.aspect_ratio_items.empty()) {
        result.ratios = default_presets;
        return result;
    }

    std::vector<Common::Types::RatioPreset> configured_ratios;
    std::wstring error_details;

    for (const auto& item_str : config.menu.aspect_ratio_items) {
        bool found = false;
        for (const auto& preset : default_presets) {
            if (item_str == preset.name) {
                configured_ratios.push_back(preset);
                found = true;
                break;
            }
        }
        if (!found) {
            if (!add_custom_ratio(item_str, configured_ratios)) {
                error_details += item_str + L", ";
            }
        }
    }

    if (!configured_ratios.empty()) {
        result.ratios = configured_ratios;
    } else {
        result.ratios = default_presets;
    }

    if (!error_details.empty()) {
        error_details.resize(error_details.length() - 2); // remove trailing ", "
        result.success = false;
        result.error_details = strings.CONFIG_FORMAT_ERROR + L" " + error_details + L"\n" + strings.RATIO_FORMAT_EXAMPLE;
    }

    return result;
}

auto get_resolution_presets(const AppConfig& config, const Constants::LocalizedStrings& strings) -> ResolutionLoadResult {
    ResolutionLoadResult result;
    auto default_presets = get_default_resolution_presets();

    if (config.menu.resolution_items.empty()) {
        result.resolutions = default_presets;
        return result;
    }
    
    std::vector<Common::Types::ResolutionPreset> configured_resolutions;
    std::wstring error_details;

    for (const auto& item_str : config.menu.resolution_items) {
        bool found = false;
        for (const auto& preset : default_presets) {
            if (item_str == preset.name) {
                configured_resolutions.push_back(preset);
                found = true;
                break;
            }
        }
        if (!found) {
            if (!add_custom_resolution(item_str, configured_resolutions)) {
                error_details += item_str + L", ";
            }
        }
    }

    if (!configured_resolutions.empty()) {
        result.resolutions = configured_resolutions;
    } else {
        result.resolutions = default_presets;
    }

    if (!error_details.empty()) {
        error_details.resize(error_details.length() - 2);
        result.success = false;
        result.error_details = strings.CONFIG_FORMAT_ERROR + L" " + error_details + L"\n" + strings.RESOLUTION_FORMAT_EXAMPLE;
    }

    return result;
}

} // namespace Core::Config::Io 