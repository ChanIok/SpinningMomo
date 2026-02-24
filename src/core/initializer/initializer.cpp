module;

module Core.Initializer;

import std;
import Core.Async;
import Core.WorkerPool;
import Core.State;
import Core.State.RuntimeInfo;
import Core.HttpServer;
import Core.Events;
import Core.Events.State;
import Core.Events.Registrar;
import Core.RPC.Registry;
import Core.Initializer.Database;
import Core.Migration;
import Features.Gallery;
import Features.Settings;
import Core.Commands;
import Core.Commands.State;
import Features.Settings.State;
import Features.Recording;
import Features.ReplayBuffer;
import Features.ReplayBuffer.State;
import Features.ReplayBuffer.UseCase;
import Features.Update;
import Features.VirtualGamepad;
import Features.Letterbox.State;
import UI.FloatingWindow;
import UI.FloatingWindow.State;
import UI.WebViewWindow;
import UI.TrayIcon;
import UI.ContextMenu;
import Vendor.Windows;
import Utils.Logger;

namespace Core::Initializer {

auto initialize_application(Core::State::AppState& state, Vendor::Windows::HINSTANCE instance)
    -> std::expected<void, std::string> {
  try {
    Core::Events::register_all_handlers(state);

    if (auto result = Core::Async::start(*state.async); !result) {
      return std::unexpected("Failed to start async runtime: " + result.error());
    }

    if (auto result = Core::WorkerPool::start(*state.worker_pool); !result) {
      return std::unexpected("Failed to start worker pool: " + result.error());
    }

    Core::RPC::Registry::register_all_endpoints(state);

    if (auto result = Core::HttpServer::initialize(state); !result) {
      return std::unexpected("Failed to initialize HTTP server: " + result.error());
    }

    if (auto db_result = Core::Initializer::Database::initialize_database(state); !db_result) {
      return std::unexpected("Failed to initialize database: " + db_result.error());
    }

    if (!Core::Migration::run_migration_if_needed(state)) {
      return std::unexpected("Application migration failed. Please check logs for details.");
    }

    if (auto settings_result = Features::Settings::initialize(state); !settings_result) {
      return std::unexpected("Failed to initialize settings: " + settings_result.error());
    }

    // 从 settings 同步 letterbox 启用状态
    state.letterbox->enabled = state.settings->raw.features.letterbox.enabled;

    if (auto update_result = Features::Update::initialize(state); !update_result) {
      return std::unexpected("Failed to initialize update: " + update_result.error());
    }

    // 初始化命令注册表
    Core::Commands::register_builtin_commands(state, state.commands->registry);
    Logger().info("Command registry initialized with {} commands",
                  state.commands->registry.descriptors.size());

    if (auto result = UI::FloatingWindow::create_window(state); !result) {
      return std::unexpected("Failed to create app window: " + result.error());
    }

    // Set up notify_hwnd for event system wake-up
    state.events->notify_hwnd = state.floating_window->window.hwnd;

    if (auto result = UI::TrayIcon::create(state); !result) {
      return std::unexpected("Failed to create tray icon: " + result.error());
    }

    if (auto result = UI::ContextMenu::initialize(state); !result) {
      return std::unexpected("Failed to initialize tray menu: " + result.error());
    }

    if (auto result = Features::Recording::initialize(*state.recording); !result) {
      return std::unexpected(result.error());
    }

    if (auto result = Features::ReplayBuffer::initialize(*state.replay_buffer); !result) {
      return std::unexpected("Failed to initialize replay buffer: " + result.error());
    }

    if (auto gallery_result = Features::Gallery::initialize(state); !gallery_result) {
      return std::unexpected("Failed to initialize gallery: " + gallery_result.error());
    }

    // 初始化虚拟手柄（检测 ViGEm 可用性，不自动启用）
    if (auto vg_result = Features::VirtualGamepad::initialize(state); !vg_result) {
      Logger().warn("Virtual gamepad initialization failed: {}", vg_result.error());
      // 不返回错误，许应用继续运行
    }

    const bool should_open_onboarding =
        Features::Settings::should_show_onboarding(state.settings->raw);
    if (should_open_onboarding) {
      Logger().info("Onboarding required, attempting to open main UI window");
      UI::WebViewWindow::activate_window(state);
    } else {
      // 默认显示悬浮窗
      UI::FloatingWindow::show_window(state);
    }

    // 注册所有命令的热键
    Core::Commands::register_all_hotkeys(state, state.floating_window->window.hwnd);

    Logger().info("Application initialized successfully");
    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Exception during initialization: " + std::string(e.what()));
  }
}

}  // namespace Core::Initializer
