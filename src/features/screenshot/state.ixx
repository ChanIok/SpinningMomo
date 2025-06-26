module;

#include <d3d11.h>
#include <windows.h>

#include <thread>

export module Features.Screenshot.State;

import std;
import Utils.Timer;
import Utils.Graphics.D3D;
import Utils.Graphics.Capture;
import Vendor.Windows;

export namespace Features::Screenshot::State {

// 截图请求结构体
struct ScreenshotRequest {
  Vendor::Windows::HWND target_window = nullptr;
  std::wstring file_path;
  std::function<void(bool success, const std::wstring& path)> completion_callback;
  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

// 会话信息结构体
struct SessionInfo {
  Utils::Graphics::Capture::CaptureSession session;
  ScreenshotRequest request;
  std::chrono::steady_clock::time_point created_time = std::chrono::steady_clock::now();
};

// 截图系统状态
struct ScreenshotState {
  // D3D资源
  std::optional<Utils::Graphics::D3D::D3DContext> d3d_context;
  winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrt_device{nullptr};
  bool d3d_initialized = false;

  // 工作线程管理
  std::unique_ptr<std::jthread> worker_thread;
  std::condition_variable worker_cv;
  std::mutex worker_mutex;
  std::atomic<bool> should_stop{false};
  std::atomic<bool> cleanup_requested{false};

  // 请求队列
  std::queue<ScreenshotRequest> pending_requests;
  std::mutex request_mutex;

  // 活跃的捕获会话管理
  std::unordered_map<size_t, SessionInfo> active_sessions;
  std::atomic<size_t> next_session_id{1};

  // 清理定时器
  std::optional<Utils::Timer::Timer> cleanup_timer;

  // 请求D3D资源清理（线程安全）
  auto request_d3d_cleanup() -> void {
    cleanup_requested = true;
    worker_cv.notify_one();  // 唤醒工作线程处理清理
  }

  // 清理活跃的捕获会话
  auto cleanup_active_sessions() -> void {
    for (auto& [session_id, session_info] : active_sessions) {
      Utils::Graphics::Capture::stop_capture(session_info.session);
      Utils::Graphics::Capture::cleanup_capture_session(session_info.session);

      // 通知调用者会话被取消
      if (session_info.request.completion_callback) {
        session_info.request.completion_callback(false, session_info.request.file_path);
      }
    }
    active_sessions.clear();
  }

  // 清理D3D资源（仅在工作线程中调用）
  auto cleanup_d3d_resources() -> void {
    cleanup_active_sessions();
    winrt_device = nullptr;
    if (d3d_context) {
      Utils::Graphics::D3D::cleanup_d3d_context(*d3d_context);
      d3d_context.reset();
    }
    d3d_initialized = false;
  }

  // 停止工作线程
  auto shutdown_worker() -> void {
    should_stop = true;
    worker_cv.notify_all();
    if (worker_thread && worker_thread->joinable()) {
      worker_thread->join();
    }
    worker_thread.reset();
  }
};

}  // namespace Features::Screenshot::State