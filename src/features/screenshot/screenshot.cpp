module;

#include <wil/result.h>

module Features.Screenshot;

import std;
import Core.State;
import Core.State.AppInfo;
import Features.Screenshot.State;
import Utils.Logger;
import Utils.Path;
import Utils.String;
import Utils.Graphics.Capture;
import Utils.Graphics.D3D;
import Utils.Image;
import <d3d11.h>;
import <wil/com.h>;
import <wincodec.h>;
import <windows.h>;

namespace Features::Screenshot {

// WIC 编码保存纹理
auto save_texture_with_wic(ID3D11Texture2D* texture, const std::wstring& file_path)
    -> std::expected<void, std::string> {
  try {
    if (!texture) {
      return std::unexpected("Texture cannot be null");
    }

    // 获取纹理描述
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);

    // 获取设备和上下文
    wil::com_ptr<ID3D11Device> device;
    texture->GetDevice(device.put());
    THROW_HR_IF_NULL(E_POINTER, device);

    wil::com_ptr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.put());
    THROW_HR_IF_NULL(E_POINTER, context);

    // 创建暂存纹理
    D3D11_TEXTURE2D_DESC staging_desc = desc;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.BindFlags = 0;
    staging_desc.MiscFlags = 0;
    staging_desc.ArraySize = 1;
    staging_desc.MipLevels = 1;

    wil::com_ptr<ID3D11Texture2D> staging_texture;
    THROW_IF_FAILED(device->CreateTexture2D(&staging_desc, nullptr, staging_texture.put()));

    // 复制纹理数据
    context->CopyResource(staging_texture.get(), texture);

    // 映射纹理并写入像素数据
    D3D11_MAPPED_SUBRESOURCE mapped{};
    THROW_IF_FAILED(context->Map(staging_texture.get(), 0, D3D11_MAP_READ, 0, &mapped));

    // 使用 RAII 确保纹理总是被正确解除映射
    auto unmap_on_exit = wil::scope_exit([&] { context->Unmap(staging_texture.get(), 0); });

    // 创建WIC工厂
    auto wic_factory_result = Utils::Image::create_factory();
    if (!wic_factory_result) {
      return std::unexpected("Failed to create WIC imaging factory: " + wic_factory_result.error());
    }
    auto wic_factory = wic_factory_result.value();

    auto save_result = Utils::Image::save_pixel_data_to_file(
        wic_factory.get(), static_cast<const uint8_t*>(mapped.pData), desc.Width, desc.Height,
        mapped.RowPitch, file_path);

    if (!save_result) {
      return std::unexpected(save_result.error());
    }

    return {};
  } catch (const wil::ResultException& e) {
    return std::unexpected(std::format("WIC texture save failed: {}", e.what()));
  }
}

// 安全调用完成回调的辅助函数
auto safe_call_completion_callback(const Features::Screenshot::State::ScreenshotRequest& request,
                                   bool success) -> void {
  if (!request.completion_callback) {
    return;
  }

  try {
    request.completion_callback(success, request.file_path);
  } catch (...) {
    Logger().error("Exception in completion callback");
  }
}

// 核心截图捕获逻辑
auto do_screenshot_capture(const Features::Screenshot::State::ScreenshotRequest& request,
                           Features::Screenshot::State::ScreenshotState& state)
    -> std::expected<void, std::string> {
  try {
    // 获取窗口大小
    RECT rect;
    THROW_IF_WIN32_BOOL_FALSE(GetWindowRect(request.target_window, &rect));

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
      return std::unexpected("Invalid window size");
    }

    // 生成唯一的会话ID
    auto session_id = state.next_session_id.fetch_add(1);

    // 创建帧回调，通过会话ID管理生命周期
    auto frame_callback = [&state,
                           session_id](Utils::Graphics::Capture::Direct3D11CaptureFrame frame) {
      bool success = false;

      // 查找对应的会话信息
      auto it = state.active_sessions.find(session_id);
      if (it == state.active_sessions.end()) {
        Logger().error("Session {} not found in frame callback", session_id);
        return;
      }

      auto& session_info = it->second;

      // 如果使用了手动隐藏光标，在这里恢复光标显示
      if (session_info.session.need_hide_cursor) {
        ShowCursor(TRUE);
      }

      if (frame) {
        auto surface = frame.Surface();
        if (surface) {
          auto texture =
              Utils::Graphics::Capture::get_dxgi_interface_from_object<ID3D11Texture2D>(surface);
          if (texture) {
            // 直接在回调中保存纹理
            auto save_result = save_texture_with_wic(texture.get(), session_info.request.file_path);
            if (save_result) {
              success = true;
              Logger().debug("Screenshot saved successfully for session {}", session_id);
            } else {
              Logger().error("Failed to save screenshot for session {}: {}", session_id,
                             save_result.error());
            }
          }
        }
      } else {
        Logger().error("Captured frame is null for session {}", session_id);
      }

      // 停止并清理捕获会话
      Utils::Graphics::Capture::stop_capture(session_info.session);
      Utils::Graphics::Capture::cleanup_capture_session(session_info.session);

      // 调用完成回调
      safe_call_completion_callback(session_info.request, success);

      // 从活跃会话中移除
      state.active_sessions.erase(it);
      Logger().debug("Session {} completed and removed", session_id);
    };

    // 创建捕获会话
    auto session_result = Utils::Graphics::Capture::create_capture_session(
        request.target_window, state.winrt_device, width, height, frame_callback);
    if (!session_result) {
      return std::unexpected("Failed to create capture session: " + session_result.error());
    }

    // 创建会话信息并存储到状态中
    Features::Screenshot::State::SessionInfo session_info;
    session_info.session = std::move(session_result.value());
    session_info.request = request;

    // 如果需要手动隐藏光标，则在开始捕获前隐藏光标
    if (session_info.session.need_hide_cursor) {
      ShowCursor(FALSE);
    }

    state.active_sessions[session_id] = std::move(session_info);

    // 开始捕获 - 不等待，直接返回
    auto start_result =
        Utils::Graphics::Capture::start_capture(state.active_sessions[session_id].session);
    if (!start_result) {
      // 如果启动失败，清理会话，恢复光标显示
      if (state.active_sessions[session_id].session.need_hide_cursor) {
        ShowCursor(TRUE);
      }
      state.active_sessions.erase(session_id);
      return std::unexpected("Failed to start capture: " + start_result.error());
    }

    Logger().debug("Screenshot capture started for session {}", session_id);
    return {};
  } catch (const wil::ResultException& e) {
    return std::unexpected(std::format("Screenshot capture failed: {}", e.what()));
  }
}

// 处理单个截图请求
auto process_single_request(const Features::Screenshot::State::ScreenshotRequest& request,
                            Core::State::AppState& app_state) -> void {
  auto& state = *app_state.screenshot;
  Logger().debug("Processing screenshot request for window: {}",
                 reinterpret_cast<uintptr_t>(request.target_window));

  try {
    auto result = do_screenshot_capture(request, state);
    if (result) {
      Logger().debug("Screenshot capture started successfully");
    } else {
      Logger().error("Failed to start screenshot capture: {}", result.error());
      safe_call_completion_callback(request, false);
    }
  } catch (...) {
    Logger().error("Exception during screenshot capture");
    safe_call_completion_callback(request, false);
  }
}

// 启动清理定时器
auto start_cleanup_timer(Features::Screenshot::State::ScreenshotState& state) -> void {
  if (!state.d3d_initialized) {
    return;
  }

  if (!state.cleanup_timer) {
    state.cleanup_timer.emplace();
  }

  if (state.cleanup_timer->IsRunning()) {
    state.cleanup_timer->Cancel();
  }

  auto result = state.cleanup_timer->SetTimer(std::chrono::milliseconds(5000), [&state]() {
    Logger().debug("Screenshot cleanup timer triggered");
    state.request_d3d_cleanup();  // 请求清理而不是直接清理
  });

  if (!result) {
    Logger().error("Failed to set screenshot cleanup timer");
  } else {
    Logger().debug("Screenshot cleanup timer started (5 seconds)");
  }
}

// 工作线程主函数
auto worker_thread_proc(Core::State::AppState& app_state) -> void {
  auto& state = *app_state.screenshot;
  Logger().debug("Screenshot worker thread started");

  while (!state.should_stop) {
    Features::Screenshot::State::ScreenshotRequest request;
    bool has_request = false;

    // 等待新请求或清理请求
    {
      std::unique_lock<std::mutex> lock(state.worker_mutex);
      state.worker_cv.wait(lock, [&state]() {
        std::lock_guard<std::mutex> req_lock(state.request_mutex);
        return state.should_stop || !state.pending_requests.empty() ||
               state.cleanup_requested.load();
      });

      if (state.should_stop) {
        break;
      }

      // 优先处理清理请求
      if (state.cleanup_requested.load()) {
        // 确保没有活跃会话时才清理
        if (state.active_sessions.empty()) {
          Logger().debug("Processing D3D cleanup request");
          state.cleanup_d3d_resources();
          state.cleanup_requested = false;
          Logger().debug("D3D resources cleaned up by worker thread");
        } else {
          Logger().debug("Cleanup requested but active sessions exist, deferring cleanup");
        }
        continue;  // 继续下一轮循环
      }

      // 获取正常请求
      std::lock_guard<std::mutex> req_lock(state.request_mutex);
      if (!state.pending_requests.empty()) {
        request = state.pending_requests.front();
        state.pending_requests.pop();
        has_request = true;
      }
    }

    // 处理请求
    if (has_request) {
      process_single_request(request, app_state);

      // 如果队列为空，启动清理定时器
      {
        std::lock_guard<std::mutex> req_lock(state.request_mutex);
        if (state.pending_requests.empty()) {
          start_cleanup_timer(state);
        }
      }
    }
  }

  Logger().debug("Screenshot worker thread stopped");
}

// 只初始化D3D资源（不创建工作线程）
auto initialize_d3d_resources_only(Core::State::AppState& app_state)
    -> std::expected<void, std::string> {
  try {
    auto& state = *app_state.screenshot;
    Logger().debug("Initializing D3D resources only");

    // 检查系统支持
    if (!app_state.app_info->is_capture_supported) {
      return std::unexpected("Windows Graphics Capture is not supported");
    }

    // 使用 WIL 的 RAII COM 初始化
    // 这会在函数退出时自动调用 CoUninitialize，并正确处理 RPC_E_CHANGED_MODE
    auto co_init = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);

    // 创建无头D3D设备（不需要窗口和交换链）
    auto d3d_result = Utils::Graphics::D3D::create_headless_d3d_device();
    if (!d3d_result) {
      return std::unexpected("Failed to create headless D3D device: " + d3d_result.error());
    }

    // 创建一个简化的D3DContext，只包含设备和上下文
    Utils::Graphics::D3D::D3DContext context;
    context.device = d3d_result->first;
    context.context = d3d_result->second;
    // 注意：swap_chain 和 render_target 保持为空，因为截图不需要它们

    state.d3d_context = std::move(context);

    // 创建WinRT设备
    auto winrt_result =
        Utils::Graphics::Capture::create_winrt_device(state.d3d_context->device.get());
    if (!winrt_result) {
      state.cleanup_d3d_resources();
      return std::unexpected("Failed to create WinRT device: " + winrt_result.error());
    }

    state.winrt_device = std::move(*winrt_result);
    state.d3d_initialized = true;

    Logger().debug("D3D resources initialized successfully");
    return {};
  } catch (const wil::ResultException& e) {
    return std::unexpected(std::format("D3D initialization failed: {}", e.what()));
  }
}

// 初始化完整系统
auto initialize_system(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  auto& state = *app_state.screenshot;
  Logger().debug("Initializing screenshot system");

  // 初始化D3D资源
  auto d3d_result = initialize_d3d_resources_only(app_state);
  if (!d3d_result) {
    return d3d_result;
  }

  // 启动工作线程
  state.should_stop = false;
  state.worker_thread =
      std::make_unique<std::jthread>([&app_state]() { worker_thread_proc(app_state); });

  // 清空队列
  std::lock_guard<std::mutex> lock(state.request_mutex);
  while (!state.pending_requests.empty()) {
    state.pending_requests.pop();
  }

  Logger().debug("Screenshot system initialized successfully");
  return {};
}

auto cleanup_system(Core::State::AppState& app_state) -> void {
  auto& state = *app_state.screenshot;
  Logger().debug("Cleaning up screenshot system");

  // 取消清理定时器
  if (state.cleanup_timer && state.cleanup_timer->IsRunning()) {
    state.cleanup_timer->Cancel();
  }

  // 停止工作线程
  state.shutdown_worker();

  // 清空待处理请求
  {
    std::lock_guard<std::mutex> lock(state.request_mutex);
    while (!state.pending_requests.empty()) {
      auto& request = state.pending_requests.front();
      safe_call_completion_callback(request, false);
      state.pending_requests.pop();
    }
  }

  // 清理D3D资源
  state.cleanup_d3d_resources();

  Logger().debug("Screenshot system cleaned up");
}

auto take_screenshot(
    Core::State::AppState& app_state, HWND target_window,
    std::function<void(bool success, const std::wstring& path)> completion_callback)
    -> std::expected<void, std::string> {
  auto& state = *app_state.screenshot;
  if (!target_window || !IsWindow(target_window)) {
    return std::unexpected("Invalid target window handle");
  }

  // 生成截图文件路径
  auto exe_dir_result = Utils::Path::GetExecutableDirectory();
  if (!exe_dir_result) {
    return std::unexpected("Failed to get executable directory: " + exe_dir_result.error());
  }

  auto screenshots_dir = exe_dir_result.value() / "screenshots";
  auto ensure_result = Utils::Path::EnsureDirectoryExists(screenshots_dir);
  if (!ensure_result) {
    return std::unexpected("Failed to create screenshots directory: " + ensure_result.error());
  }

  auto filename = Utils::String::FormatTimestamp(std::chrono::system_clock::now());
  auto file_path = Utils::Path::Combine(screenshots_dir, filename);

  // 自动初始化系统（如果尚未初始化）
  if (!state.d3d_initialized || !state.worker_thread) {
    // 取消任何待处理的清理请求
    state.cleanup_requested = false;

    Logger().debug("Screenshot system not initialized, initializing automatically");

    // 如果只是D3D资源被清理，但工作线程还在，只重新初始化D3D资源
    if (!state.d3d_initialized && state.worker_thread && state.worker_thread->joinable()) {
      Logger().debug("Worker thread exists, only reinitializing D3D resources");
      auto d3d_result = initialize_d3d_resources_only(app_state);
      if (!d3d_result) {
        return std::unexpected("Failed to reinitialize D3D resources: " + d3d_result.error());
      }
    } else {
      // 完全重新初始化系统
      Logger().debug("Full system reinitialization required");
      auto init_result = initialize_system(app_state);
      if (!init_result) {
        return std::unexpected("Failed to initialize screenshot system: " + init_result.error());
      }
    }
    Logger().debug("Screenshot system initialized automatically");
  }

  // 取消清理定时器和清理请求（新请求开始）
  if (state.cleanup_timer && state.cleanup_timer->IsRunning()) {
    state.cleanup_timer->Cancel();
    Logger().debug("Cancelled screenshot cleanup timer due to new request");
  }
  state.cleanup_requested = false;  // 取消任何待处理的清理请求

  // 创建截图请求
  Features::Screenshot::State::ScreenshotRequest request;
  request.target_window = target_window;
  request.file_path = file_path.wstring();
  request.completion_callback = completion_callback;
  request.timestamp = std::chrono::steady_clock::now();

  // 添加到队列并唤醒工作线程
  {
    std::lock_guard<std::mutex> lock(state.request_mutex);
    state.pending_requests.push(request);
  }

  // 唤醒工作线程
  state.worker_cv.notify_one();

  return {};
}

}  // namespace Features::Screenshot
