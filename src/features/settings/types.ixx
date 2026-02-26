module;

export module Features.Settings.Types;

import std;
import <rfl.hpp>;

namespace Features::Settings::Types {

// 当前设置版本
export constexpr int CURRENT_SETTINGS_VERSION = 1;
// 当前欢迎流程版本（用于控制是否需要重新引导）
export constexpr int CURRENT_ONBOARDING_FLOW_VERSION = 1;

// Web 主题设置
export struct WebThemeSettings {
  std::string mode = "system";              // "light" | "dark" | "system"
  std::string cjk_font_preset = "harmony";  // "harmony" | "microsoft"
};

// Web 背景设置
export struct WebBackgroundSettings {
  std::string type = "none";  // "none" | "image"
  std::string image_path = "";
  int background_blur_amount = 0;                                    // 0 - 100
  double background_opacity = 1.0;                                   // 0.0 - 1.0
  std::vector<std::string> overlay_colors = {"#000000", "#000000"};  // 1 - 4
  double overlay_opacity = 0.8;                                      // 0.0 - 1.0
  double surface_opacity = 1.0;                                      // 0.0 - 1.0
};

// 完整的应用设置（重构后的结构）
export struct AppSettings {
  int version = CURRENT_SETTINGS_VERSION;

  // app 分组 - 应用核心设置
  struct App {
    bool always_run_as_admin = true;  // 始终以管理员权限运行

    // 首次引导设置
    struct Onboarding {
      // 默认为 true，避免老用户升级时被强制进入引导；首次创建配置时会改写为 false
      bool completed = true;
      int flow_version = CURRENT_ONBOARDING_FLOW_VERSION;
    } onboarding;

    // 快捷键设置
    // 修饰键值: MOD_ALT=1, MOD_CONTROL=2, MOD_SHIFT=4, MOD_WIN=8
    struct Hotkey {
      struct FloatingWindow {
        int modifiers = 2;  // MOD_CONTROL (Ctrl)
        int key = 192;      // VK_OEM_3 (`)
      } floating_window;

      struct Screenshot {
        int modifiers = 0;  // 无修饰键
        int key = 0x7A;     // VK_F11 (F11) - PrintScreen(0x2C) 可能被系统截图功能拦截
      } screenshot;

      struct Recording {
        int modifiers = 0;  // 无修饰键
        int key = 0x77;     // VK_F8 (F8)
      } recording;
    } hotkey;

    // 语言设置
    struct Language {
      std::string current = "zh-CN";  // zh-CN, en-US
    } language;

    // 日志设置
    struct Logger {
      std::string level = "INFO";  // DEBUG, INFO, ERROR
    } logger;
  } app;

  // window 分组 - 窗口相关设置
  struct Window {
    std::string target_title = "无限暖暖  ";  // 目标窗口标题
  } window;

  // features 分组 - 功能特性设置
  struct Features {
    std::string output_dir_path = "";      // 统一输出目录（截图+录制），空=默认 Videos/SpinningMomo
    std::string external_album_path = "";  // 外部游戏相册目录路径（为空时回退到输出目录）

    // 黑边模式设置
    struct Letterbox {
      bool enabled = false;  // 是否启用黑边模式
    } letterbox;

    // 动态照片设置
    struct MotionPhoto {
      std::uint32_t duration = 3;             // 视频时长（秒）
      std::uint32_t resolution = 0;           // 短边分辨率: 0=原始不缩放, 720/1080/1440/2160
      std::uint32_t fps = 30;                 // 帧率
      std::uint32_t bitrate = 10'000'000;     // 比特率 (10Mbps)，CBR 模式使用
      std::uint32_t quality = 80;             // 质量值 (0-100)，VBR 模式使用
      std::string rate_control = "vbr";       // 码率控制模式: "cbr" | "vbr"
      std::string codec = "h264";             // 编码格式: "h264" | "h265"
      std::string audio_source = "system";    // 音频源: "none" | "system" | "game_only"
      std::uint32_t audio_bitrate = 192'000;  // 音频码率 (192kbps)
    } motion_photo;

    // 即时回放设置（录制参数继承自 recording）
    struct ReplayBuffer {
      std::uint32_t duration = 30;  // 回放时长（秒）
      // enabled 不持久化，仅运行时状态
      // 其他参数从 recording 继承
    } replay_buffer;

    // 录制功能设置
    struct Recording {
      std::uint32_t fps = 60;              // 帧率
      std::uint32_t bitrate = 80'000'000;  // 比特率 (bps)，默认 80Mbps，CBR 模式使用
      std::uint32_t quality = 80;          // 质量值 (0-100)，VBR 模式使用
      std::uint32_t qp = 23;               // 量化参数 (0-51)，ManualQP 模式使用
      std::string rate_control = "vbr";    // 码率控制模式: "cbr" | "vbr" | "manual_qp"
      std::string encoder_mode = "auto";   // 编码器模式: "auto" | "gpu" | "cpu"
      std::string codec = "h264";          // 视频编码格式: "h264" | "h265"
      bool capture_client_area = true;     // 是否只捕获客户区（无边框）
      bool capture_cursor = false;         // 是否捕获鼠标指针

      // 音频配置
      std::string audio_source = "system";    // 音频源: "none" | "system" | "game_only"
      std::uint32_t audio_bitrate = 320'000;  // 音频码率 (bps)，默认 320kbps
    } recording;
  } features;

  // update 分组 - 更新设置
  struct Update {
    bool auto_check = true;         // 是否自动检查更新
    int check_interval_hours = 24;  // 检查间隔（小时）

    // 版本检查URL（Cloudflare Pages）
    std::string version_url = "https://spinning.infinitymomo.com/version.txt";

    // 下载源配置（按优先级排序）
    struct DownloadSource {
      std::string name;          // 源名称
      std::string url_template;  // URL模板，支持 {version} 和 {filename} 占位符
    };

    std::vector<DownloadSource> download_sources = {
        {"GitHub", "https://github.com/ChanIok/SpinningMomo/releases/download/v{0}/{1}"},
        {"Mirror", "https://r2.infinitymomo.com/releases/v{0}/{1}"}};
  } update;

  // ui 分组 - UI界面设置
  struct UI {
    // 应用菜单配置
    struct AppMenu {
      // 启用的功能项（有则启用，顺序即菜单显示顺序）
      std::vector<std::string> features = {
          "screenshot.capture", "recording.toggle",   "preview.toggle",
          "overlay.toggle",     "window.reset",       "app.main",
          "app.exit",           "output.open_folder", "external_album.open_folder",
          "letterbox.toggle"};
      // 启用的比例列表（顺序即为菜单显示顺序）
      std::vector<std::string> aspect_ratios = {"21:9", "16:9", "3:2", "1:1", "3:4", "2:3", "9:16"};
      // 启用的分辨率列表（顺序即为菜单显示顺序）
      std::vector<std::string> resolutions = {"Default", "1080P", "2K", "4K", "6K", "8K", "12K"};
    } app_menu;

    // 浮窗布局配置
    struct FloatingWindowLayout {
      int base_item_height = 24;
      int base_title_height = 26;
      int base_separator_height = 0;
      int base_font_size = 12;
      int base_text_padding = 12;
      int base_indicator_width = 3;
      int base_ratio_indicator_width = 4;
      int base_ratio_column_width = 60;
      int base_resolution_column_width = 70;
      int base_settings_column_width = 80;
      int base_scroll_indicator_width = 3;  // 滚动条宽度
    } floating_window_layout;

    // 浮窗颜色配置
    struct FloatingWindowColors {
      std::string background = "#1f1f1fB3";
      std::string separator = "#333333B3";
      std::string text = "#D8D8D8FF";
      std::string indicator = "#FBBF24FF";
      std::string hover = "#505050CC";
      std::string title_bar = "#1f1f1fB3";
      std::string scroll_indicator = "#808080CC";  // 滚动条颜色
    } floating_window_colors;

    // 浮窗主题模式
    std::string floating_window_theme_mode = "dark";

    // WebView 主窗口尺寸和位置（持久化）
    // x/y 为 -1 表示未保存过，首次启动时居中
    struct WebViewWindow {
      int width = 900;
      int height = 600;
      int x = -1;
      int y = -1;
      bool enable_transparent_background = false;
    } webview_window;

    // Web UI 设置
    WebThemeSettings web_theme;
    WebBackgroundSettings background;
  } ui;

  // 插件配置
  struct Plugins {
    struct InfinityNikki {
      bool enable = true;
      std::string game_dir = "";
    } infinity_nikki;
  } plugins;
};

// 设置变更事件数据
export struct SettingsChangeData {
  AppSettings old_settings;
  AppSettings new_settings;
  std::string change_description;
};

// 获取设置
export struct GetSettingsParams {
  // 空结构体，未来可扩展
};

export using GetSettingsResult = AppSettings;

export using UpdateSettingsParams = AppSettings;

export struct UpdateSettingsResult {
  bool success;
  std::string message;
};

// 局部更新设置（JSON Merge Patch 风格，不支持字段删除）
export struct PatchSettingsParams {
  rfl::Generic::Object patch;
};

export using PatchSettingsResult = UpdateSettingsResult;

}  // namespace Features::Settings::Types
