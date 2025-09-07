module;

module Core.Shutdown;

import std;

import Core.Async;
import Core.WorkerPool;
import Core.HttpServer;
import Core.State;
import Features.Letterbox;
import Features.Overlay;
import Features.Preview;
import Features.Screenshot;
import Features.Updater;
import Features.Updater.State;
import UI.AppWindow;
import UI.ContextMenu;
import UI.TrayIcon;
import UI.WebViewWindow;
import Utils.Logger;

namespace Core::Shutdown {

auto shutdown_application(Core::State::AppState& state) -> void {
  Logger().info("Starting application shutdown sequence...");

  // 清理顺序应该与 Core::Initializer::initialize_application 中的初始化顺序相反

  // 1. UI 清理
  UI::ContextMenu::cleanup(state);
  UI::TrayIcon::destroy(state);
  UI::AppWindow::destroy_window(state);
  UI::WebViewWindow::cleanup(state);

  // 2. 功能模块清理
  // 检查是否有待处理的更新
  if (state.updater && state.updater->pending_update) {
    Logger().info("Executing pending update on program exit");
    Features::Updater::execute_pending_update(state);
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

  // 停止工作线程池（等待所有任务完成）
  Core::WorkerPool::stop(*state.worker_pool);

  Core::Async::stop(*state.async);

  Logger().info("Application shutdown sequence finished.");
}

}  // namespace Core::Shutdown
