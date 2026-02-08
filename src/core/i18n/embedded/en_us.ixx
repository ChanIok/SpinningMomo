// Auto-generated embedded English locale module
// DO NOT EDIT - This file contains embedded locale data
//
// Source: src/locales/en-US.json
// Module: Core.I18n.Embedded.EnUS
// Variable: en_us_json

module;

export module Core.I18n.Embedded.EnUS;

import std;

export namespace EmbeddedLocales {
// Embedded English JSON content as string_view
// Size: 3736 bytes
constexpr std::string_view en_us_json = R"EmbeddedJson({
  "version": "1.0",

  "menu.app_main": "Main",
  "menu.app_float": "Floating Window",
  "menu.app_exit": "Exit",
  "menu.app_user_guide": "User Guide",
  "menu.float_show": "Show Floating Window",
  "menu.float_hide": "Hide Floating Window",
  "menu.float_toggle": "Show/Hide Floating Window",

  "menu.window_select": "Select Window",
  "menu.window_no_available": "(No Available Windows)",
  "menu.window_ratio": "Window Ratio",
  "menu.window_resolution": "Resolution",
  "menu.window_reset": "Reset",
  "menu.window_toggle_borderless": "Toggle Window Border",
  "menu.window_auto_hide": "Hide Taskbar",
  "menu.window_lower_on_resize": "Lower Taskbar When Resizing",

  "menu.screenshot_capture": "Capture",
  "menu.screenshot_open_folder": "Screenshots",
  "menu.overlay_toggle": "Overlay",
  "menu.preview_toggle": "Preview",
  "menu.recording_toggle": "Record",
  "menu.motion_photo_toggle": "Motion Photo",
  "menu.replay_buffer_toggle": "Instant Replay",
  "menu.replay_buffer_save": "Save Replay",
  "menu.letterbox_toggle": "Letterbox",
  "menu.virtual_gamepad_toggle": "Virtual Gamepad",


  "menu.settings_config": "Open Config",
  "menu.settings_language": "Language",

  "message.app_startup": "Window ratio adjustment tool is running in background.\nPress [",
  "message.app_startup_suffix": "] to show/hide the adjustment window",
  "message.app_feature_not_supported": "This feature requires Windows 10 1803 or higher and has been disabled.",

  "message.window_selected": "Window Selected",
  "message.window_adjust_success": "Window adjusted successfully!",
  "message.window_adjust_failed": "Failed to adjust window. May need administrator privileges, or window doesn't support resizing.",
  "message.window_not_found": "Target window not found. Please ensure the window is running.",
  "message.window_reset_success": "Window has been reset to screen size.",
  "message.window_reset_failed": "Failed to reset window size.",

  "message.screenshot_success": "Screenshot saved to: ",
  "message.preview_overlay_conflict": "Preview Window and Overlay Window cannot be used simultaneously, and one of the functions has been automatically disabled.",
  "message.preview_start_failed": "Failed to start preview window: ",
  "message.overlay_start_failed": "Failed to start overlay window: ",
  "message.recording_start_failed": "Failed to start recording: ",
  "message.recording_stop_failed": "Failed to stop recording: ",
  "message.motion_photo_success": "Motion Photo saved: ",
  "message.replay_saved": "Replay saved: ",
  "message.motion_photo_start_failed": "Failed to start Motion Photo: ",
  "message.replay_buffer_start_failed": "Failed to start Instant Replay: ",

  "message.settings_hotkey_prompt": "Please press new hotkey combination...\nSupports Ctrl, Shift, Alt with other keys",
  "message.settings_hotkey_success": "Hotkey set to: ",
  "message.settings_hotkey_failed": "Hotkey setting failed, restored to default.",
  "message.settings_hotkey_register_failed": "Failed to register hotkey. The program can still be used, but the shortcut will not be available.",
  "message.settings_config_help": "Config File Help:\n1. [AspectRatioItems] section for custom ratios\n2. [ResolutionItems] section for custom resolutions\n3. Restart app after saving",
  "message.settings_load_failed": "Failed to load config, please check the config file.",
  "message.settings_format_error": "Format error: ",
  "message.settings_ratio_format_example": "Please use correct format, e.g.: 16:10,17:10",
  "message.settings_resolution_format_example": "Please use correct format, e.g.: 3840x2160,7680x4320",

  "label.app_name": "SpinningMomo",
  "label.language_zh_cn": "中文",
  "label.language_en_us": "English"
}
)EmbeddedJson";
}  // namespace EmbeddedLocales
