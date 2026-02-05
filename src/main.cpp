import std;
import App;
import Utils.Logger;
import Utils.System;
import Features.Settings;
import Vendor.Windows;

// 现代C++入口点
auto __stdcall wWinMain([[maybe_unused]] Vendor::Windows::HINSTANCE hInstance,
                        [[maybe_unused]] Vendor::Windows::HINSTANCE hPrevInstance,
                        [[maybe_unused]] Vendor::Windows::LPWSTR lpCmdLine,
                        [[maybe_unused]] int nCmdShow) -> int {
  // 单实例检测：防止应用多开（必须在提权检查之前）
  if (!Utils::System::acquire_single_instance_lock()) {
    // 已有实例在运行，激活它的窗口后退出
    Utils::System::activate_existing_instance();
    return 0;
  }

  // 权限检查：如果需要管理员权限且当前没有，则重启并请求提权
  if (Features::Settings::should_run_as_admin() && !Utils::System::is_process_elevated()) {
    if (Utils::System::restart_as_elevated(lpCmdLine)) {
      // 成功启动了新的提权进程，当前进程退出
      return 0;
    }
    // 用户取消了 UAC 对话框或启动失败，继续以普通权限运行
  }

  // 在所有操作之前初始化日志系统
  if (auto result = Utils::Logging::initialize(); !result) {
    // 如果日志初始化失败，使用系统API显示错误，因为我们的日志系统不可用
    const auto error_message = "Logger Failed: " + result.error();
    Vendor::Windows::MessageBoxA(nullptr, error_message.c_str(), "Fatal Error",
                                 Vendor::Windows::kMB_ICONERROR);
    return -1;
  }

  int exit_code = 0;
  // 使用 RAII 和现代C++特性
  try {
    auto app = std::make_unique<Application>();

    if (!app->Initialize(hInstance)) {
      // 现在可以安全地使用日志记录器
      Logger().critical("Failed to initialize application");
      Vendor::Windows::MessageBoxW(nullptr, L"Failed to initialize application", L"Error",
                                   Vendor::Windows::kMB_ICONERROR);
      exit_code = -1;
    } else {
      exit_code = app->Run();
    }

  } catch (const std::exception& e) {
    Logger().critical("Unhandled exception: {}", e.what());
    Vendor::Windows::MessageBoxA(nullptr, e.what(), "Fatal Error", Vendor::Windows::kMB_ICONERROR);
    exit_code = -1;
  }

  // 在应用程序退出前，确保关闭日志系统
  Utils::Logging::shutdown();
  return exit_code;
}
