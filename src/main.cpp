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

  const auto startup_settings = Features::Settings::load_startup_settings();

  // 尽早初始化日志，覆盖单实例、提权与启动早期故障
  if (auto result = Utils::Logging::initialize(startup_settings.logger_level); !result) {
    const auto error_message = "Logger Failed: " + result.error();
    Vendor::Windows::MessageBoxA(nullptr, error_message.c_str(), "Fatal Error",
                                 Vendor::Windows::kMB_ICONERROR);
    return -1;
  }

  // 单实例检查（需早于提权流程）
  if (!Utils::System::acquire_single_instance_lock()) {
    Logger().info("Existing instance detected, activating the running instance");
    // 已有实例时激活并退出
    Utils::System::activate_existing_instance();
    Utils::Logging::shutdown();
    return 0;
  }

  // 需要管理员权限时尝试提权重启
  if (startup_settings.always_run_as_admin && !Utils::System::is_process_elevated()) {
    Logger().info("Elevation required by settings, attempting restart as elevated");
    // 提权前先释放单实例锁，避免提权后的新进程误判为“已有实例”
    Utils::System::release_single_instance_lock();

    if (Utils::System::restart_as_elevated(lpCmdLine)) {
      Logger().info("Elevated process started successfully, current process exits");
      Utils::Logging::shutdown();
      // 提权进程已启动，当前进程退出
      return 0;
    }

    Logger().warn("Elevation was cancelled or failed, continuing without admin privileges");

    // 取消 UAC 或启动失败：重新获取单实例锁后继续普通权限
    if (!Utils::System::acquire_single_instance_lock()) {
      Logger().info("Existing instance detected after elevation fallback, activating it");
      Utils::System::activate_existing_instance();
      Utils::Logging::shutdown();
      return 0;
    }
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
