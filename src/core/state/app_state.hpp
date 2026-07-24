#pragma once

#include "vendor/std.hpp"

namespace core::rpc {
struct RpcState;
}

namespace core::async {
struct AsyncState;
}

namespace core::dialog_service {
struct DialogServiceState;
}

namespace core::events {
struct EventsState;
}

namespace core::i18n {
struct I18nState;
}

namespace core::webview {
struct WebViewState;
}

namespace core::runtime_info {
struct RuntimeInfoState;
}

namespace core::database {
struct DatabaseState;
}

namespace core::http_server {
struct HttpServerState;
}

namespace core::http_client {
struct HttpClientState;
}

namespace core::worker_pool {
struct WorkerPoolState;
}

namespace core::commands {
struct CommandState;
}

namespace core::tasks {
struct TaskState;
}

namespace features::settings {
struct SettingsState;
}

namespace features::update {
struct UpdateState;
}

namespace ui::shared_render_resources {
struct SharedRenderResourcesState;
}

namespace ui::floating_window {
struct FloatingWindowState;
}

namespace ui::tray_icon {
struct TrayIconState;
}

namespace ui::context_menu {
struct ContextMenuState;
}

namespace ui::notification_window {
struct NotificationWindowState;
}

namespace ui::photography_panel {
struct PhotographyPanelState;
}

namespace features::letterbox {
struct LetterboxState;
}

namespace features::gallery {
struct GalleryState;
}

namespace features::overlay {
struct OverlayState;
}

namespace features::preview {
struct PreviewState;
}

namespace features::window_control {
struct WindowControlState;
}

namespace features::screenshot {
struct ScreenshotState;
}

namespace features::recording {
struct RecordingState;
}

namespace features::photography {
struct PhotographyState;
}

namespace core {

struct AppState {
  AppState();
  ~AppState();

  // 应用级状态
  std::unique_ptr<core::rpc::RpcState> rpc;
  std::unique_ptr<core::async::AsyncState> async;
  std::unique_ptr<core::dialog_service::DialogServiceState> dialog_service;
  std::unique_ptr<core::events::EventsState> events;
  std::unique_ptr<core::i18n::I18nState> i18n;
  std::unique_ptr<core::webview::WebViewState> webview;
  std::unique_ptr<core::runtime_info::RuntimeInfoState> runtime_info;
  std::unique_ptr<core::database::DatabaseState> database;
  std::unique_ptr<core::http_server::HttpServerState> http_server;
  std::unique_ptr<core::http_client::HttpClientState> http_client;
  std::unique_ptr<core::worker_pool::WorkerPoolState> worker_pool;
  std::unique_ptr<core::commands::CommandState> commands;
  std::unique_ptr<core::tasks::TaskState> tasks;

  // 应用设置状态（包含配置和计算状态）
  std::unique_ptr<features::settings::SettingsState> settings;

  // 更新模块状态
  std::unique_ptr<features::update::UpdateState> update;

  // UI状态
  std::unique_ptr<ui::shared_render_resources::SharedRenderResourcesState> shared_render_resources;
  std::unique_ptr<ui::floating_window::FloatingWindowState> floating_window;
  std::unique_ptr<ui::tray_icon::TrayIconState> tray_icon;
  std::unique_ptr<ui::context_menu::ContextMenuState> context_menu;
  std::unique_ptr<ui::notification_window::NotificationWindowState> notification_window;
  std::unique_ptr<ui::photography_panel::PhotographyPanelState> photography_panel;

  // 功能模块状态
  std::unique_ptr<features::letterbox::LetterboxState> letterbox;
  std::unique_ptr<features::gallery::GalleryState> gallery;
  std::unique_ptr<features::overlay::OverlayState> overlay;
  std::unique_ptr<features::preview::PreviewState> preview;
  std::unique_ptr<features::window_control::WindowControlState> window_control;
  std::unique_ptr<features::screenshot::ScreenshotState> screenshot;
  std::unique_ptr<features::recording::RecordingState> recording;
  std::unique_ptr<features::photography::PhotographyState> photography;
};

}  // namespace core
