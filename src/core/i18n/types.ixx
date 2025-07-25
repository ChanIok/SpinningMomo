module;

export module Core.I18n.Types;

import std;

export namespace Core::I18n::Types {

enum class Language { ZhCN, EnUS };

// 简化的二层结构文本数据
struct TextData {
  std::string version = "1.0";

  // 菜单项文本 - 所有出现在菜单中的可点击项
  struct Menu {
    // 应用菜单项
    std::string app_show;        // 显示浮窗
    std::string app_hide;        // 关闭浮窗
    std::string app_exit;        // 退出
    std::string app_user_guide;  // 使用指南
    std::string app_webview;     // 主界面

    // 窗口菜单项
    std::string window_select;             // 选择窗口
    std::string window_no_available;       // 无可用窗口
    std::string window_ratio;              // 窗口比例
    std::string window_resolution;         // 分辨率
    std::string window_reset;              // 重置窗口
    std::string window_toggle_borderless;  // 切换窗口边框
    std::string window_auto_hide;          // 隐藏任务栏
    std::string window_lower_on_resize;    // 调整时置底任务栏

    // 功能菜单项
    std::string screenshot_capture;      // 截图
    std::string screenshot_open_folder;  // 打开相册
    std::string overlay_toggle;          // 叠加层
    std::string preview_toggle;          // 预览窗
    std::string letterbox_toggle;        // 黑边模式

    // 设置菜单项
    std::string settings_hotkey;    // 修改热键
    std::string settings_config;    // 打开配置文件
    std::string settings_language;  // 语言
  } menu;

  // 提示消息文本 - 所有反馈给用户的状态信息
  struct Message {
    // 应用消息
    std::string app_startup;                // 启动提示
    std::string app_startup_suffix;         // 启动提示后缀
    std::string app_feature_not_supported;  // 功能不支持

    // 窗口消息
    std::string window_selected;        // 已选择窗口
    std::string window_adjust_success;  // 窗口调整成功
    std::string window_adjust_failed;   // 窗口调整失败
    std::string window_not_found;       // 未找到目标窗口
    std::string window_reset_success;   // 窗口已重置
    std::string window_reset_failed;    // 重置窗口失败

    // 功能消息
    std::string screenshot_success;  // 截图成功
    std::string overlay_conflict;    // 叠加层冲突
    std::string preview_conflict;    // 预览窗冲突

    // 设置消息
    std::string settings_hotkey_prompt;              // 热键设置提示
    std::string settings_hotkey_success;             // 热键设置成功
    std::string settings_hotkey_failed;              // 热键设置失败
    std::string settings_hotkey_register_failed;     // 热键注册失败
    std::string settings_config_help;                // 配置文件帮助
    std::string settings_load_failed;                // 配置加载失败
    std::string settings_format_error;               // 格式错误
    std::string settings_ratio_format_example;       // 比例格式示例
    std::string settings_resolution_format_example;  // 分辨率格式示例
  } message;

  // 标签文本 - 所有静态显示的标识文本
  struct Label {
    // 应用标签
    std::string app_name;  // 应用名称

    // 语言标签
    std::string language_zh_cn;  // 中文
    std::string language_en_us;  // English
  } label;
};

// 辅助函数：创建默认文本数据
inline auto create_default_text_data() -> TextData {
  TextData text_data;
  text_data.version = "1.0";

  // 设置一些默认值
  text_data.label.app_name = "SpinningMomo";
  text_data.label.language_zh_cn = "中文";
  text_data.label.language_en_us = "English";

  return text_data;
}

}  // namespace Core::I18n::Types