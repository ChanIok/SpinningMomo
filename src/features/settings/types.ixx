module;

export module Features.Settings.Types;

import std;

export namespace Features::Settings::Types {

struct WindowSettings {
  std::string title;
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