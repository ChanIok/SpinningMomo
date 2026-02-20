// Auto-generated embedded Chinese locale module
// DO NOT EDIT - This file contains embedded locale data
//
// Source: src/locales/zh-CN.json
// Module: Core.I18n.Embedded.ZhCN
// Variable: zh_cn_json

module;

export module Core.I18n.Embedded.ZhCN;

import std;

export namespace EmbeddedLocales {
// Embedded Chinese JSON content as string_view
// Size: 3470 bytes
constexpr std::string_view zh_cn_json = R"EmbeddedJson({
  "version": "1.0",

  "menu.app_main": "主界面",
  "menu.app_float": "悬浮窗",
  "menu.app_exit": "退出",
  "menu.app_user_guide": "使用指南",
  "menu.float_show": "显示悬浮窗",
  "menu.float_hide": "隐藏悬浮窗",
  "menu.float_toggle": "显示/隐藏悬浮窗",

  "menu.window_select": "选择窗口",
  "menu.window_no_available": "(无可用窗口)",
  "menu.window_ratio": "窗口比例",
  "menu.window_resolution": "分辨率",
  "menu.window_reset": "重置窗口",
  "menu.window_toggle_borderless": "切换窗口边框",

  "menu.screenshot_capture": "截图",
  "menu.screenshot_open_folder": "打开相册",
  "menu.overlay_toggle": "叠加层",
  "menu.preview_toggle": "预览窗",
  "menu.recording_toggle": "录制",
  "menu.motion_photo_toggle": "动态照片",
  "menu.replay_buffer_toggle": "即时回放",
  "menu.replay_buffer_save": "保存回放",
  "menu.letterbox_toggle": "黑边模式",
  "menu.virtual_gamepad_toggle": "虚拟手柄",


  "menu.settings_config": "打开配置文件",
  "menu.settings_language": "语言",

  "message.app_startup": "窗口比例调整工具已在后台运行。\n按 [",
  "message.app_startup_suffix": "] 可以显示/隐藏调整窗口",
  "message.app_feature_not_supported": "此功能需要 Windows 10 1803 或更高版本，已自动禁用。",

  "message.window_selected": "已选择窗口",
  "message.window_adjust_success": "窗口调整成功！",
  "message.window_adjust_failed": "窗口调整失败。可能需要管理员权限，或窗口不支持调整大小。",
  "message.window_not_found": "未找到目标窗口，请确保窗口已启动。",
  "message.window_reset_success": "窗口已重置为屏幕大小。",
  "message.window_reset_failed": "重置窗口尺寸失败。",

  "message.screenshot_success": "截图成功，已保存至: ",
  "message.preview_overlay_conflict": "预览窗和叠加层功能冲突，已自动关闭另一功能",
  "message.preview_start_failed": "预览窗启动失败: ",
  "message.overlay_start_failed": "叠加层启动失败: ",
  "message.recording_start_failed": "录制启动失败: ",
  "message.recording_stop_failed": "录制停止失败: ",
  "message.motion_photo_success": "动态照片已保存: ",
  "message.replay_saved": "回放已保存: ",
  "message.motion_photo_start_failed": "动态照片启动失败: ",
  "message.replay_buffer_start_failed": "即时回放启动失败: ",

  "message.settings_hotkey_prompt": "请按下新的热键组合...\n支持 Ctrl、Shift、Alt 组合其他按键",
  "message.settings_hotkey_success": "热键已设置为：",
  "message.settings_hotkey_failed": "热键设置失败，已恢复默认热键。",
  "message.settings_hotkey_register_failed": "热键注册失败。程序仍可使用，但快捷键将不可用。",
  "message.settings_config_help": "配置文件说明：\n1. [AspectRatioItems] 节用于添加自定义比例\n2. [ResolutionItems] 节用于添加自定义分辨率\n3. 保存后重启软件生效",
  "message.settings_load_failed": "加载配置失败，请检查配置文件。",
  "message.settings_format_error": "格式错误：",
  "message.settings_ratio_format_example": "请使用正确格式，如：16:10,17:10",
  "message.settings_resolution_format_example": "请使用正确格式，如：3840x2160,7680x4320",

  "label.app_name": "旋转吧大喵",
  "label.language_zh_cn": "中文",
  "label.language_en_us": "English"
}
)EmbeddedJson";
}  // namespace EmbeddedLocales
