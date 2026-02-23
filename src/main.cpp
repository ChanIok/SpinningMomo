import std;
import App;
import Utils.CrashDump;
import Utils.Logger;
import Utils.System;
import Features.Settings;
import Vendor.Windows;

// Win32 入口
auto __stdcall wWinMain(Vendor::Windows::HINSTANCE hInstance,
                        [[maybe_unused]] Vendor::Windows::HINSTANCE hPrevInstance,
                        Vendor::Windows::LPWSTR lpCmdLine, [[maybe_unused]] int nCmdShow) -> int {
  // 尽早安装崩溃转储处理器
  Utils::CrashDump::install();

  // 单实例检查（需早于提权流程）
  if (!Utils::System::acquire_single_instance_lock()) {
    // 已有实例时激活并退出
    Utils::System::activate_existing_instance();
    return 0;
  }

  // 需要管理员权限时尝试提权重启
  if (Features::Settings::should_run_as_admin() && !Utils::System::is_process_elevated()) {
    if (Utils::System::restart_as_elevated(lpCmdLine)) {
      // 提权进程已启动，当前进程退出
      return 0;
    }
    // 取消 UAC 或启动失败则继续普通权限
  }

  // 初始化日志系统
  if (auto result = Utils::Logging::initialize(); !result) {
    // 日志不可用时直接弹窗
    const auto error_message = "Logger Failed: " + result.error();
    Vendor::Windows::MessageBoxA(nullptr, error_message.c_str(), "Fatal Error",
                                 Vendor::Windows::kMB_ICONERROR);
    return -1;
  }

  int exit_code = 0;
  // 主流程
  try {
    Application app;

    if (!app.Initialize(hInstance)) {
      Logger().critical("Failed to initialize application");
      Vendor::Windows::MessageBoxW(nullptr, L"Failed to initialize application", L"Error",
                                   Vendor::Windows::kMB_ICONERROR);
      exit_code = -1;
    } else {
      exit_code = app.Run();
    }

  } catch (const std::exception& e) {
    Logger().critical("Unhandled exception: {}", e.what());
    Vendor::Windows::MessageBoxA(nullptr, e.what(), "Fatal Error", Vendor::Windows::kMB_ICONERROR);
    exit_code = -1;
  }

  // 退出前关闭日志
  Utils::Logging::shutdown();
  return exit_code;
}
