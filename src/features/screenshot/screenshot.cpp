#include "features/screenshot/screenshot.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d3d11.hpp"
#include "vendor/windows/wincodec.hpp"
#include "vendor/windows/winrt/windows_graphics_capture.hpp"

#include "core/state/app_state.hpp"
#include "core/state/runtime_info.hpp"
#include "features/screenshot/hdr_encoder.hpp"
#include "features/screenshot/state.hpp"
#include "features/settings/state.hpp"
#include "utils/graphics/capture.hpp"
#include "utils/graphics/capture_region.hpp"
#include "utils/graphics/d3d.hpp"
#include "utils/graphics/hdr.hpp"
#include "utils/graphics/photo_processing.hpp"
#include "utils/image/image.hpp"
#include "utils/logger/logger.hpp"
#include "utils/path/path.hpp"
#include "utils/string/string.hpp"

namespace features::screenshot {

auto start_cleanup_timer(features::screenshot::ScreenshotState& state) -> void;

// WIC 编码保存纹理
auto save_texture_with_wic(ID3D11Texture2D* texture, const std::wstring& file_path,
                           utils::image::ImageFormat format = utils::image::ImageFormat::PNG,
                           float jpeg_quality = 1.0f) -> std::expected<void, std::string> {
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
    auto wic_factory_result = utils::image::create_factory();
    if (!wic_factory_result) {
      return std::unexpected("Failed to create WIC imaging factory: " + wic_factory_result.error());
    }
    auto wic_factory = wic_factory_result.value();

    auto save_result = utils::image::save_pixel_data_to_file(
        wic_factory.get(), static_cast<const uint8_t*>(mapped.pData), desc.Width, desc.Height,
        mapped.RowPitch, file_path, format, jpeg_quality);

    if (!save_result) {
      return std::unexpected(save_result.error());
    }

    return {};
  } catch (const wil::ResultException& e) {
    return std::unexpected(std::format("WIC texture save failed: {}", e.what()));
  }
}

auto make_failed_save_result(const features::screenshot::ScreenshotRequest& request,
                             std::string error) -> features::screenshot::ScreenshotSaveResult {
  const bool jxr_requested = request.use_hdr && request.save_jxr;
  return features::screenshot::ScreenshotSaveResult{
      .success = false,
      .path = request.file_path,
      .error = std::move(error),
      .jxr_requested = jxr_requested,
      .jxr_path = request.jxr_file_path,
      .jxr_error = jxr_requested ? "Screenshot capture failed" : "",
  };
}

// 安全调用完成回调的辅助函数
auto safe_call_completion_callback(features::screenshot::ScreenshotRequest& request,
                                   features::screenshot::ScreenshotSaveResult result) -> void {
  if (!request.completion_callback) {
    return;
  }

  try {
    auto completion_callback = std::move(request.completion_callback);
    completion_callback(std::move(result));
  } catch (...) {
    Logger().error("Exception in completion callback");
  }
}

// 根据 HDR 标记保存主图；HDR + JXR 请求会在同一条 D3D 线程上提交两路 GPU 准备，
// 然后让 JXR 的 WIC 编码与 Ultra HDR 的后续 GPU/CPU 工作重叠。
auto save_capture_textures(ID3D11Texture2D* texture,
                           const features::screenshot::ScreenshotRequest& request)
    -> features::screenshot::ScreenshotSaveResult {
  const bool jxr_requested = request.use_hdr && request.save_jxr;
  features::screenshot::ScreenshotSaveResult result{
      .path = request.file_path,
      .jxr_requested = jxr_requested,
      .jxr_path = request.jxr_file_path,
  };

  if (!texture) {
    result.error = "Texture cannot be null";
    if (jxr_requested) {
      result.jxr_error = result.error;
    }
    return result;
  }

  const auto hdr_options = features::screenshot::hdr_encoder::UltraHdrEncodeOptions{
      .target_display_peak_nits = request.hdr_target_peak_nits};

  // 非 HDR 或未开启 JXR 时保持单路旧流程；JXR 只对有效 HDR 请求生效。
  if (!jxr_requested) {
    try {
      auto primary_result = request.use_hdr
                                ? features::screenshot::hdr_encoder::save_texture_as_ultrahdr_jpeg(
                                      texture, request.file_path, hdr_options)
                                : save_texture_with_wic(texture, request.file_path, request.format,
                                                        request.jpeg_quality);
      if (primary_result) {
        result.success = true;
      } else {
        result.error = primary_result.error();
      }
    } catch (const std::exception& e) {
      result.error = std::format("Screenshot output failed: {}", e.what());
    } catch (...) {
      result.error = "Screenshot output failed";
    }
    return result;
  }

  // 1. 先创建 JXR staging texture 并提交 CopyResource，只在稍后 Map。
  std::optional<features::screenshot::hdr_encoder::JxrReadbackSession> jxr_readback;
  try {
    auto jxr_readback_result = features::screenshot::hdr_encoder::begin_jxr_readback(texture);
    if (jxr_readback_result) {
      jxr_readback.emplace(std::move(jxr_readback_result.value()));
    } else {
      result.jxr_error = jxr_readback_result.error();
    }
  } catch (const std::exception& e) {
    result.jxr_error = std::format("JPEG XR readback setup failed: {}", e.what());
  } catch (...) {
    result.jxr_error = "JPEG XR readback setup failed";
  }

  // 2. 紧接着提交 Ultra HDR 直方图；此时两路 GPU 准备都已经进入同一个 immediate context。
  std::optional<features::screenshot::hdr_encoder::UltraHdrPreprocessSession> hdr_session;
  try {
    auto hdr_session_result = features::screenshot::hdr_encoder::begin_ultrahdr_preprocess(texture);
    if (hdr_session_result) {
      hdr_session.emplace(std::move(hdr_session_result.value()));
    } else {
      result.error = hdr_session_result.error();
    }
  } catch (const std::exception& e) {
    result.error = std::format("Ultra HDR preprocess setup failed: {}", e.what());
  } catch (...) {
    result.error = "Ultra HDR preprocess setup failed";
  }

  std::optional<std::future<std::expected<void, std::string>>> jxr_future;

  if (!jxr_readback) {
    if (result.jxr_error.empty()) {
      result.jxr_error = "JPEG XR readback is unavailable";
    }
  } else {
    // 3. 在 D3D 线程完成 staging Map，复制成独立 CPU 缓冲后立即释放 D3D 资源。
    try {
      auto jxr_pixels_result =
          features::screenshot::hdr_encoder::read_jxr_pixels(std::move(*jxr_readback));
      if (!jxr_pixels_result) {
        result.jxr_error = jxr_pixels_result.error();
      } else {
        // 4. WIC/文件写入只使用 CPU 像素，交给独立线程；该线程不触碰 D3D immediate context。
        jxr_future.emplace(std::async(
            std::launch::async,
            [pixels = std::move(jxr_pixels_result.value()), file_path = request.jxr_file_path]() {
              return features::screenshot::hdr_encoder::save_jxr_pixels(pixels, file_path);
            }));
      }
    } catch (const std::exception& e) {
      result.jxr_error = std::format("JPEG XR readback or encoding setup failed: {}", e.what());
    } catch (...) {
      result.jxr_error = "JPEG XR readback or encoding setup failed";
    }
  }

  // 5. 主线程读取直方图并继续 Ultra HDR GPU 预处理、读回和 JPEG 编码。
  if (!hdr_session) {
    if (result.error.empty()) {
      result.error = "Ultra HDR preprocess is unavailable";
    }
  } else {
    try {
      auto prepared_result =
          features::screenshot::hdr_encoder::finish_ultrahdr_preprocess(*hdr_session);
      if (!prepared_result) {
        result.error = prepared_result.error();
      } else {
        auto primary_result =
            features::screenshot::hdr_encoder::save_prepared_images_as_ultrahdr_jpeg(
                prepared_result.value(), request.file_path, hdr_options);
        if (primary_result) {
          result.success = true;
        } else {
          result.error = primary_result.error();
        }
      }
    } catch (const std::exception& e) {
      result.error = std::format("Ultra HDR output failed: {}", e.what());
    } catch (...) {
      result.error = "Ultra HDR output failed";
    }
  }

  // 6. 类似 Promise.allSettled：无论主图是否失败，都等待已启动的 JXR 任务并汇总结果。
  if (jxr_future) {
    try {
      auto jxr_result = jxr_future->get();
      if (jxr_result) {
        result.jxr_success = true;
      } else {
        result.jxr_error = jxr_result.error();
      }
    } catch (const std::exception& e) {
      result.jxr_error = std::format("JPEG XR encoding task failed: {}", e.what());
    } catch (...) {
      result.jxr_error = "JPEG XR encoding task failed";
    }
  }

  return result;
}

// 截图完成收尾：恢复光标 → 停止捕获 → 回调 → 移除会话 → 检查是否启动空闲清理
auto finish_screenshot_session(
    features::screenshot::ScreenshotState& state,
    std::unordered_map<size_t, features::screenshot::SessionInfo>::iterator session_it,
    size_t session_id, features::screenshot::ScreenshotSaveResult result) -> void {
  auto& session_info = session_it->second;

  if (session_info.session.need_hide_cursor) {
    ShowCursor(TRUE);
  }

  utils::graphics::capture::stop_capture(session_info.session);
  utils::graphics::capture::cleanup_capture_session(session_info.session);
  safe_call_completion_callback(session_info.request, std::move(result));
  state.active_sessions.erase(session_it);
  Logger().debug("Session {} completed and removed", session_id);

  {
    std::lock_guard<std::mutex> lock(state.request_mutex);
    if (state.pending_requests.empty() && state.active_sessions.empty()) {
      start_cleanup_timer(state);
    }
  }
}

// 核心截图捕获逻辑
auto do_screenshot_capture(features::screenshot::ScreenshotRequest& request,
                           features::screenshot::ScreenshotState& state)
    -> std::expected<void, std::string> {
  try {
    // 最小化窗口不执行截图，避免创建无法完成的捕获会话
    if (IsIconic(request.target_window)) {
      return std::unexpected("Target window is minimized");
    }

    // 获取 WGC 真实的捕获宽高以消除阴影引起的黑边
    auto capture_size_result =
        utils::graphics::capture::get_capture_item_size(request.target_window);
    if (!capture_size_result) {
      return std::unexpected("Failed to get capture item size: " + capture_size_result.error());
    }

    int width = capture_size_result->first;
    int height = capture_size_result->second;
    if (width <= 0 || height <= 0) {
      return std::unexpected("Invalid window size");
    }

    // 生成唯一的会话ID
    auto session_id = state.next_session_id.fetch_add(1);

    // 创建帧回调，通过会话ID管理生命周期
    auto frame_callback = [&state,
                           session_id](utils::graphics::capture::Direct3D11CaptureFrame frame) {
      // 查找对应的会话信息
      auto it = state.active_sessions.find(session_id);
      if (it == state.active_sessions.end()) {
        Logger().error("Session {} not found in frame callback", session_id);
        return;
      }

      auto& session_info = it->second;
      auto save_result = make_failed_save_result(session_info.request, "Captured frame is null");

      if (frame) {
        auto surface = frame.Surface();
        if (surface) {
          auto texture =
              utils::graphics::capture::get_dxgi_interface_from_object<ID3D11Texture2D>(surface);
          if (texture) {
            ID3D11Texture2D* texture_to_save = texture.get();
            const int shutter_frames = std::max(0, session_info.request.shutter_frames);
            if (shutter_frames > 0) {
              // 首帧初始化 GPU 均值累积器，后续帧做加权混合
              if (!session_info.average_accumulator) {
                auto accumulator_result =
                    utils::graphics::photo_processing::initialize_average_accumulator(
                        texture.get());
                if (!accumulator_result) {
                  Logger().error("Failed to initialize long exposure for session {}: {}",
                                 session_id, accumulator_result.error());
                  finish_screenshot_session(
                      state, it, session_id,
                      make_failed_save_result(session_info.request, accumulator_result.error()));
                  return;
                }
                session_info.average_accumulator = std::move(accumulator_result.value());
              } else {
                auto accumulate_result =
                    utils::graphics::photo_processing::accumulate_average_frame(
                        *session_info.average_accumulator, texture.get());
                if (!accumulate_result) {
                  Logger().error("Failed to accumulate long exposure for session {}: {}",
                                 session_id, accumulate_result.error());
                  finish_screenshot_session(
                      state, it, session_id,
                      make_failed_save_result(session_info.request, accumulate_result.error()));
                  return;
                }
              }

              // 未达到目标帧数时直接返回，等下一帧继续累积
              if (session_info.average_accumulator->frame_count <
                  static_cast<std::uint32_t>(shutter_frames)) {
                return;
              }

              // 累积完成，用混合后的均值纹理替换原始帧
              texture_to_save = session_info.average_accumulator->current_average.get();
            }

            // 若开启了无边框捕获（仅捕获客户区），则在保存前裁剪纹理
            wil::com_ptr<ID3D11Texture2D> cropped_texture;
            if (session_info.request.capture_client_area) {
              D3D11_TEXTURE2D_DESC desc;
              texture_to_save->GetDesc(&desc);

              auto crop_region_result =
                  utils::graphics::capture_region::calculate_client_crop_region(
                      session_info.request.target_window, desc.Width, desc.Height);
              if (crop_region_result) {
                wil::com_ptr<ID3D11Device> device;
                texture_to_save->GetDevice(device.put());
                wil::com_ptr<ID3D11DeviceContext> context;
                if (device) {
                  device->GetImmediateContext(context.put());
                }
                if (device && context) {
                  auto crop_result = utils::graphics::capture_region::crop_texture_to_region(
                      device.get(), context.get(), texture_to_save, *crop_region_result,
                      cropped_texture);
                  if (crop_result) {
                    texture_to_save = crop_result.value();
                  } else {
                    Logger().warn("Failed to crop texture for screenshot: {}", crop_result.error());
                  }
                }
              } else {
                Logger().warn("Failed to calculate client crop region for screenshot: {}",
                              crop_region_result.error());
              }
            }

            save_result = save_capture_textures(texture_to_save, session_info.request);
            if (save_result.success) {
              if (session_info.request.use_hdr) {
                Logger().info("HDR screenshot saved for session {}: {}", session_id,
                              utils::string::ToUtf8(session_info.request.file_path));
              } else if (shutter_frames > 0) {
                Logger().debug("Long exposure screenshot saved successfully for session {}",
                               session_id);
              } else {
                Logger().debug("Screenshot saved successfully for session {}", session_id);
              }
            } else {
              if (session_info.request.use_hdr) {
                Logger().error("HDR screenshot save failed for session {}: {}", session_id,
                               save_result.error);
              } else {
                Logger().error("Failed to save screenshot for session {}: {}", session_id,
                               save_result.error);
              }
            }
            if (save_result.jxr_requested && !save_result.jxr_success) {
              Logger().error("HDR JPEG XR save failed for session {}: {}", session_id,
                             save_result.jxr_error);
            }
          } else {
            save_result.error = "Failed to get captured texture";
            if (save_result.jxr_requested) {
              save_result.jxr_error = save_result.error;
            }
          }
        } else {
          save_result.error = "Captured frame surface is null";
          if (save_result.jxr_requested) {
            save_result.jxr_error = save_result.error;
          }
        }
      } else {
        Logger().error("Captured frame is null for session {}", session_id);
      }

      finish_screenshot_session(state, it, session_id, std::move(save_result));
    };

    // 创建捕获会话
    utils::graphics::capture::CaptureSessionOptions capture_options;
    // 默认可捕获 8-bit BGRA；HDR 截图需要半精度浮点帧池才能保留高光动态范围。
    if (request.use_hdr) {
      capture_options.pixel_format =
          winrt::Windows::Graphics::DirectX::DirectXPixelFormat::R16G16B16A16Float;
    }

    auto session_result = utils::graphics::capture::create_capture_session(
        request.target_window, state.winrt_device, width, height, frame_callback, 1,
        capture_options);
    if (!session_result) {
      return std::unexpected("Failed to create capture session: " + session_result.error());
    }

    // 先在注册表中创建槽位；分配失败时 request 尚未移动，调用方仍能完成失败通知。
    auto [session_it, inserted] = state.active_sessions.try_emplace(session_id);
    if (!inserted) {
      return std::unexpected("Screenshot session id already exists");
    }
    auto& session_info = session_it->second;
    session_info.session = std::move(session_result.value());
    session_info.request = std::move(request);

    // 如果需要手动隐藏光标，则在开始捕获前隐藏光标
    if (session_info.session.need_hide_cursor) {
      ShowCursor(FALSE);
    }

    // 开始捕获 - 不等待，直接返回
    auto start_result = utils::graphics::capture::start_capture(session_info.session);
    if (!start_result) {
      // request 已归活动会话所有；启动失败由这里完成通知和资源回收。
      if (session_info.session.need_hide_cursor) {
        ShowCursor(TRUE);
      }
      utils::graphics::capture::cleanup_capture_session(session_info.session);
      safe_call_completion_callback(
          session_info.request,
          make_failed_save_result(session_info.request, start_result.error()));
      state.active_sessions.erase(session_it);
      return std::unexpected("Failed to start capture: " + start_result.error());
    }

    Logger().debug("Screenshot capture started for session {}", session_id);
    return {};
  } catch (const wil::ResultException& e) {
    return std::unexpected(std::format("Screenshot capture failed: {}", e.what()));
  }
}

// 处理单个截图请求
auto process_single_request(features::screenshot::ScreenshotRequest request,
                            core::AppState& app_state) -> void {
  auto& state = *app_state.screenshot;
  Logger().debug("Processing screenshot request for window: {}",
                 reinterpret_cast<uintptr_t>(request.target_window));

  try {
    auto result = do_screenshot_capture(request, state);
    if (result) {
      Logger().debug("Screenshot capture started successfully");
    } else {
      Logger().error("Failed to start screenshot capture: {}", result.error());
      safe_call_completion_callback(request, make_failed_save_result(request, result.error()));
    }
  } catch (...) {
    Logger().error("Exception during screenshot capture");
    safe_call_completion_callback(
        request, make_failed_save_result(request, "Exception during screenshot capture"));
  }
}

// 启动清理定时器
auto start_cleanup_timer(features::screenshot::ScreenshotState& state) -> void {
  if (!state.d3d_initialized) {
    return;
  }

  if (!state.cleanup_timer) {
    state.cleanup_timer.emplace();
  }

  if (state.cleanup_timer->is_pending()) {
    state.cleanup_timer->cancel();
  }

  auto result = state.cleanup_timer->set_timeout(std::chrono::milliseconds(5000), [&state]() {
    Logger().debug("Screenshot cleanup timer triggered");
    state.cleanup_requested = true;
    state.worker_cv.notify_one();
  });

  if (!result) {
    Logger().error("Failed to set screenshot cleanup timer");
  } else {
    Logger().debug("Screenshot cleanup timer started (5 seconds)");
  }
}

// 清理活跃会话与 D3D 资源
auto cleanup_d3d_resources(features::screenshot::ScreenshotState& state) -> void {
  for (auto& [session_id, session_info] : state.active_sessions) {
    if (session_info.session.need_hide_cursor) {
      ShowCursor(TRUE);
    }

    utils::graphics::capture::stop_capture(session_info.session);
    utils::graphics::capture::cleanup_capture_session(session_info.session);

    if (session_info.request.completion_callback) {
      auto completion_callback = std::move(session_info.request.completion_callback);
      const bool jxr_requested = session_info.request.use_hdr && session_info.request.save_jxr;
      completion_callback(ScreenshotSaveResult{
          .success = false,
          .path = session_info.request.file_path,
          .error = "Screenshot session was cancelled",
          .jxr_requested = jxr_requested,
          .jxr_path = session_info.request.jxr_file_path,
          .jxr_error = jxr_requested ? "Screenshot session was cancelled" : "",
      });
    }
  }
  state.active_sessions.clear();

  state.winrt_device = nullptr;
  if (state.d3d_context) {
    utils::graphics::d3d::cleanup_d3d_context(*state.d3d_context);
    state.d3d_context.reset();
  }
  state.d3d_initialized = false;
}

// 工作线程主函数
auto worker_thread_proc(core::AppState& app_state) -> void {
  auto& state = *app_state.screenshot;
  Logger().debug("Screenshot worker thread started");

  while (!state.should_stop) {
    features::screenshot::ScreenshotRequest request;
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
          cleanup_d3d_resources(state);
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
        request = std::move(state.pending_requests.front());
        state.pending_requests.pop();
        has_request = true;
      }
    }

    // 处理请求
    if (has_request) {
      process_single_request(std::move(request), app_state);

      // 只有「请求已发出但未形成活跃会话」时（例如启动捕获失败）才在这里启动定时器；
      // 正常路径在帧回调保存完成后再启动，见 do_screenshot_capture 内 frame_callback。
      {
        std::lock_guard<std::mutex> req_lock(state.request_mutex);
        if (state.pending_requests.empty() && state.active_sessions.empty()) {
          start_cleanup_timer(state);
        }
      }
    }
  }

  Logger().debug("Screenshot worker thread stopped");
}

// 只初始化D3D资源（不创建工作线程）
auto initialize_d3d_resources_only(core::AppState& app_state) -> std::expected<void, std::string> {
  try {
    auto& state = *app_state.screenshot;
    Logger().debug("Initializing D3D resources only");

    // 检查系统支持
    if (!app_state.runtime_info->is_capture_supported) {
      return std::unexpected("Windows Graphics Capture is not supported");
    }

    // 使用 WIL 的 RAII COM 初始化
    // 这会在函数退出时自动调用 CoUninitialize，并正确处理 RPC_E_CHANGED_MODE
    auto co_init = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);

    // 创建无头D3D设备（不需要窗口和交换链）
    auto d3d_result = utils::graphics::d3d::create_headless_d3d_device();
    if (!d3d_result) {
      return std::unexpected("Failed to create headless D3D device: " + d3d_result.error());
    }

    // 创建一个简化的D3DContext，只包含设备和上下文
    utils::graphics::d3d::D3DContext context;
    context.device = d3d_result->first;
    context.context = d3d_result->second;
    // 注意：swap_chain 和 render_target 保持为空，因为截图不需要它们

    state.d3d_context = std::move(context);

    // 创建WinRT设备
    auto winrt_result =
        utils::graphics::capture::create_winrt_device(state.d3d_context->device.get());
    if (!winrt_result) {
      cleanup_d3d_resources(state);
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
auto initialize_system(core::AppState& app_state) -> std::expected<void, std::string> {
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

auto cleanup_system(core::AppState& app_state) -> void {
  auto& state = *app_state.screenshot;
  Logger().debug("Cleaning up screenshot system");

  // 取消清理定时器
  if (state.cleanup_timer && state.cleanup_timer->is_pending()) {
    state.cleanup_timer->cancel();
  }

  // 停止工作线程
  state.should_stop = true;
  state.worker_cv.notify_all();
  if (state.worker_thread && state.worker_thread->joinable()) {
    state.worker_thread->join();
  }
  state.worker_thread.reset();

  // 清空待处理请求
  {
    std::lock_guard<std::mutex> lock(state.request_mutex);
    while (!state.pending_requests.empty()) {
      auto& request = state.pending_requests.front();
      safe_call_completion_callback(
          request, make_failed_save_result(request, "Screenshot system was cleaned up"));
      state.pending_requests.pop();
    }
  }

  // 清理D3D资源
  cleanup_d3d_resources(state);

  Logger().debug("Screenshot system cleaned up");
}

auto take_screenshot(core::AppState& app_state, HWND target_window,
                     std::move_only_function<void(ScreenshotSaveResult result)> completion_callback,
                     utils::image::ImageFormat format, float jpeg_quality,
                     std::optional<std::filesystem::path> output_dir_override, int shutter_frames,
                     bool capture_client_area) -> std::expected<void, std::string> {
  auto& state = *app_state.screenshot;
  if (!target_window || !IsWindow(target_window)) {
    return std::unexpected("Invalid target window handle");
  }
  if (IsIconic(target_window)) {
    return std::unexpected("Target window is minimized");
  }

  // 生成截图文件路径
  std::filesystem::path screenshots_dir;

  if (output_dir_override.has_value()) {
    screenshots_dir = *output_dir_override;
    auto ensure_result = utils::path::EnsureDirectoryExists(screenshots_dir);
    if (!ensure_result) {
      return std::unexpected("Failed to create output directory: " + ensure_result.error());
    }
  } else {
    auto output_dir_result =
        utils::path::GetOutputDirectory(app_state.settings->raw.features.output_dir_path);
    if (!output_dir_result) {
      return std::unexpected("Failed to get output directory: " + output_dir_result.error());
    }
    screenshots_dir = output_dir_result.value();
  }

  auto filename = utils::string::FormatTimestamp(std::chrono::system_clock::now());
  // 用户打开「HDR 截图」且当前输出处于 HDR10 路径时，走 Ultra HDR；否则维持原有 8-bit 流程。
  bool use_hdr = false;
  float hdr_target_peak_nits = 1000.0f;
  if (app_state.settings->raw.features.screenshot.enable_hdr) {
    auto hdr_info = utils::graphics::hdr::query_monitor_hdr_info(target_window);
    if (hdr_info) {
      use_hdr = hdr_info->hdr_active;
      hdr_target_peak_nits = hdr_info->max_luminance_nits;
    } else {
      Logger().warn("Failed to query HDR monitor info: {}", hdr_info.error());
    }
  }

  // Ultra HDR 仅 JPEG 容器；Motion Photo 也用 .jpg，统一在这里改扩展名。
  if (use_hdr || format == utils::image::ImageFormat::JPEG) {
    auto dot_pos = filename.rfind('.');
    if (dot_pos != std::string::npos) {
      filename = filename.substr(0, dot_pos) + ".jpg";
    }
  }
  auto file_path = screenshots_dir / std::filesystem::path(filename);

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
  if (state.cleanup_timer && state.cleanup_timer->is_pending()) {
    state.cleanup_timer->cancel();
    Logger().debug("Cancelled screenshot cleanup timer due to new request");
  }
  state.cleanup_requested = false;  // 取消任何待处理的清理请求

  // 创建截图请求
  features::screenshot::ScreenshotRequest request;
  request.target_window = target_window;
  request.file_path = file_path.wstring();
  // 与落盘格式一致：HDR 路径实际为 JPEG，避免 request.format 仍为 PNG 导致语义错乱。
  request.format = use_hdr ? utils::image::ImageFormat::JPEG : format;
  request.jpeg_quality = jpeg_quality;
  request.use_hdr = use_hdr;
  request.save_jxr = use_hdr && app_state.settings->raw.features.screenshot.save_jxr;
  if (request.save_jxr) {
    auto jxr_file_path = file_path;
    jxr_file_path.replace_extension(L".jxr");
    request.jxr_file_path = jxr_file_path.wstring();
  }
  request.hdr_target_peak_nits = hdr_target_peak_nits;
  request.completion_callback = std::move(completion_callback);
  request.timestamp = std::chrono::steady_clock::now();
  request.shutter_frames = std::max(0, shutter_frames);
  request.capture_client_area = capture_client_area;

  if (use_hdr) {
    RECT window_rect{};
    if (GetWindowRect(target_window, &window_rect)) {
      const int capture_width = window_rect.right - window_rect.left;
      const int capture_height = window_rect.bottom - window_rect.top;
      Logger().info("HDR screenshot requested: {}x{}, target_peak={:.0f} nits, path={}",
                    capture_width, capture_height, hdr_target_peak_nits,
                    utils::string::ToUtf8(file_path.wstring()));
    } else {
      Logger().info("HDR screenshot requested: target_peak={:.0f} nits, path={}",
                    hdr_target_peak_nits, utils::string::ToUtf8(file_path.wstring()));
    }
  }

  // 添加到队列并唤醒工作线程
  {
    std::lock_guard<std::mutex> lock(state.request_mutex);
    state.pending_requests.push(std::move(request));
  }

  // 唤醒工作线程
  state.worker_cv.notify_one();

  return {};
}

}  // namespace features::screenshot
