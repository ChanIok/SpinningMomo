module;

export module Features.Settings.Types;

import std;

export namespace Features::Settings::Types {

struct WindowSettings {
  std::string title;
};

// 应用设置状态（内存中的状态）
struct AppSettings {
  WindowSettings window;
  std::string version = "1.0";
};

// 设置变更事件数据
struct SettingsChangeData {
  AppSettings old_settings;
  AppSettings new_settings;
  std::string change_description;
};

// RPC 参数和结果类型
struct GetSettingsParams {
  // 空结构体，未来可扩展
};

struct GetSettingsResult {
  WindowSettings window;
  std::string version = "1.0";
};

struct UpdateSettingsParams {
  WindowSettings window;
};

struct UpdateSettingsResult {
  bool success;
  std::string message;
};

// 业务错误类型
struct ServiceError {
  std::string message;
};

}  // namespace Features::Settings::Types 