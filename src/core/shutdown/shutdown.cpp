module;

module Core.Shutdown;

import std;

import Core.Async;
import Core.DialogService;
import Core.WorkerPool;
import Core.HttpServer;
import Core.HttpClient;
import Core.Commands;
import Core.Database;
import Core.State;
import Features.Letterbox;
import Features.Overlay;
import Features.Preview;
import Features.Screenshot;
import Features.WindowControl;
import Features.Recording.UseCase;
import Features.Update;
import Features.Update.State;
import Features.Gallery;
import Extensions.InfinityNikki.PhotoService;
import UI.FloatingWindow;
import UI.FloatingWindow.State;
import UI.ContextMenu;
import UI.NotificationWindow;
import UI.TrayIcon;
import UI.WebViewWindow;
import Utils.Logger;

namespace Core::Shutdown {

// 按初始化的反向依赖关闭应用，先收敛后台任务再释放 UI 和核心服务。
auto shutdown_application(Core::State::AppState& state) -> void {
  Logger().info("==================================================");
  Logger().info("SpinningMomo shutdown begin");
  Logger().info("==================================================");

  // 先卸载键盘、鼠标钩子，避免后续清理过程输入事件被拦截
  Core::Commands::uninstall_keyboard_keepalive_hook(state);

  Core::Commands::unregister_all_hotkeys(state, state.floating_window->window.hwnd);

  Features::WindowControl::stop_center_lock_monitor(state);

  // 清理顺序应该与 Core::Initializer::initialize_application 中的初始化顺序相反
  // 先停止录制并等待录制切换线程结束，避免与后续 UI/核心清理并发
  Features::Recording::UseCase::stop_recording_if_running(state);

  Core::DialogService::stop(state);

  auto shutdown_gallery_extensions = [](Core::State::AppState& app_state) {
    Extensions::InfinityNikki::PhotoService::shutdown(app_state);
  };
  Features::Gallery::cleanup(state, std::move(shutdown_gallery_extensions));

  // 1. UI 清理
  UI::ContextMenu::cleanup(state);
  UI::TrayIcon::destroy(state);
  UI::NotificationWindow::cleanup(state);
  UI::FloatingWindow::destroy_window(state);
  UI::WebViewWindow::cleanup(state);

  // 2. 功能模块清理
  // 检查是否有待处理的更新
  if (state.update->pending_update) {
    Logger().info("Executing pending update on program exit");
    Features::Update::execute_pending_update(state);
  }
  Features::Preview::stop_preview(state);
  Features::Preview::cleanup_preview(state);
  Features::Overlay::stop_overlay(state);
  Features::Overlay::cleanup_overlay(state);
  if (auto result = Features::Letterbox::shutdown(state); !result) {
    Logger().error("Failed to shutdown Letterbox: {}", result.error());
  }
  Features::Screenshot::cleanup_system(state);
  // 3. 核心服务清理
  Core::HttpServer::shutdown(state);
  Core::HttpClient::shutdown(state);

  // 停止工作线程池（等待所有任务完成）
  Core::WorkerPool::stop(state);

  Core::Database::shutdown(state);

  Core::Async::stop(state);

  Logger().info("==================================================");
  Logger().info("SpinningMomo shutdown complete");
  Logger().info("==================================================");
}

}  // namespace Core::Shutdown
