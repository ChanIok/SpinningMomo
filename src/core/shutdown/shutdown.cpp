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
import Features.Recording.UseCase;
import Features.Update;
import Features.Update.State;
import Features.Gallery;
import Features.VirtualGamepad;
import UI.FloatingWindow;
import UI.ContextMenu;
import UI.TrayIcon;
import UI.WebViewWindow;
import Utils.Logger;

namespace Core::Shutdown {

auto shutdown_application(Core::State::AppState& state) -> void {
  Logger().info("Starting application shutdown sequence...");

  // 清理顺序应该与 Core::Initializer::initialize_application 中的初始化顺序相反

  // 先停止录制并等待录制切换线程结束，避免与后续 UI/核心清理并发
  Features::Recording::UseCase::stop_recording_if_running(state);

  // 1. UI 清理
  UI::ContextMenu::cleanup(state);
  UI::TrayIcon::destroy(state);
  UI::FloatingWindow::destroy_window(state);
  UI::WebViewWindow::cleanup(state);

  // 2. 功能模块清理
  // 检查是否有待处理的更新
  if (state.update && state.update->pending_update) {
    Logger().info("Executing pending update on program exit");
    Features::Update::execute_pending_update(state);
  }
  Features::Preview::stop_preview(state);
  Features::Preview::cleanup_preview(state);
  Features::Overlay::stop_overlay(state);
  Features::Overlay::cleanup_overlay(state);
  Features::Gallery::cleanup(state);
  if (auto result = Features::Letterbox::shutdown(state); !result) {
    Logger().error("Failed to shutdown Letterbox: {}", result.error());
  }
  Features::Screenshot::cleanup_system(state);
  Features::VirtualGamepad::shutdown(state);

  // 3. 核心服务清理
  Core::HttpServer::shutdown(state);

  // 停止工作线程池（等待所有任务完成）
  Core::WorkerPool::stop(*state.worker_pool);

  Core::Async::stop(*state.async);

  Logger().info("Application shutdown sequence finished.");
}

}  // namespace Core::Shutdown
