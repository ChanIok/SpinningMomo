module;

#include <windows.h>

#include <iostream>

module Features.Overlay.Utils;

import std;
import Core.State;
import Features.Overlay.State;

namespace Features::Overlay::Utils {

namespace {
const char* vertex_shader_code = R"(
        struct VS_INPUT {
            float2 pos : POSITION;
            float2 tex : TEXCOORD;
        };
        struct PS_INPUT {
            float4 pos : SV_POSITION;
            float2 tex : TEXCOORD;
        };
        PS_INPUT main(VS_INPUT input) {
            PS_INPUT output;
            output.pos = float4(input.pos, 0.0f, 1.0f);
            output.tex = input.tex;
            return output;
        }
    )";

const char* pixel_shader_code = R"(
        Texture2D tex : register(t0);
        SamplerState samp : register(s0);
        float4 main(float4 pos : SV_POSITION, float2 texCoord : TEXCOORD) : SV_Target {
            return tex.Sample(samp, texCoord);
        }
    )";

// 全局状态指针，用于窗口过程回调
Core::State::AppState* g_app_state = nullptr;

// 窗口过程
LRESULT CALLBACK overlay_window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  if (g_app_state) {
    return handle_overlay_window_message(hwnd, message, wParam, lParam, *g_app_state);
  }
  return DefWindowProc(hwnd, message, wParam, lParam);
}
}  // namespace

auto get_vertex_shader_code() -> std::string_view { return vertex_shader_code; }

auto get_pixel_shader_code() -> std::string_view { return pixel_shader_code; }

auto register_overlay_window_class(HINSTANCE instance) -> std::expected<void, std::string> {
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = overlay_window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = L"OverlayWindowClass";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

  if (!RegisterClassExW(&wc)) {
    DWORD error = GetLastError();
    return std::unexpected(
        std::format("Failed to register overlay window class. Error: {}", error));
  }

  return {};
}

auto unregister_overlay_window_class(HINSTANCE instance) -> void {
  UnregisterClassW(L"OverlayWindowClass", instance);
}

auto get_screen_dimensions() -> std::pair<int, int> {
  return {GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
}

auto calculate_overlay_dimensions(int game_width, int game_height, int screen_width,
                                  int screen_height) -> std::pair<int, int> {
  double aspect_ratio = static_cast<double>(game_width) / game_height;

  int window_width, window_height;
  if (game_width * screen_height <= screen_width * game_height) {
    // 基于高度计算宽度
    window_height = screen_height;
    window_width = static_cast<int>(screen_height * aspect_ratio);
  } else {
    // 基于宽度计算高度
    window_width = screen_width;
    window_height = static_cast<int>(screen_width / aspect_ratio);
  }

  return {window_width, window_height};
}

auto should_use_overlay(int game_width, int game_height, int screen_width, int screen_height)
    -> bool {
  return game_width > screen_width || game_height > screen_height;
}

auto get_window_dimensions(HWND hwnd) -> std::expected<std::pair<int, int>, std::string> {
  RECT rect;
  if (!GetWindowRect(hwnd, &rect)) {
    DWORD error = GetLastError();
    return std::unexpected(std::format("Failed to get window rect. Error: {}", error));
  }

  return std::make_pair(rect.right - rect.left, rect.bottom - rect.top);
}

auto get_window_rect_safe(HWND hwnd) -> std::expected<RECT, std::string> {
  RECT rect;
  if (!GetWindowRect(hwnd, &rect)) {
    DWORD error = GetLastError();
    return std::unexpected(std::format("Failed to get window rect. Error: {}", error));
  }
  return rect;
}

auto set_window_transparency(HWND hwnd, BYTE alpha) -> std::expected<void, std::string> {
  LONG ex_style = GetWindowLong(hwnd, GWL_EXSTYLE);
  if (!SetWindowLong(hwnd, GWL_EXSTYLE, ex_style | WS_EX_LAYERED)) {
    DWORD error = GetLastError();
    return std::unexpected(std::format("Failed to set layered window style. Error: {}", error));
  }

  if (!SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA)) {
    DWORD error = GetLastError();
    return std::unexpected(std::format("Failed to set window transparency. Error: {}", error));
  }

  return {};
}

auto set_window_layered_attributes(HWND hwnd) -> std::expected<void, std::string> {
  if (!SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA)) {
    DWORD error = GetLastError();
    return std::unexpected(
        std::format("Failed to set layered window attributes. Error: {}", error));
  }
  return {};
}

auto handle_overlay_window_message(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                   Core::State::AppState& state) -> LRESULT {
  auto& overlay_state = *state.overlay;

  switch (message) {
    case Features::Overlay::State::WM_SHOW_OVERLAY: {
      // 显示叠加层窗口
      ShowWindow(hwnd, SW_SHOW);
      overlay_state.window.is_visible = true;
      return 0;
    }

    case Features::Overlay::State::WM_GAME_WINDOW_FOREGROUND: {
      // 处理游戏窗口前台事件
      if (overlay_state.window.target_window) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
        SetWindowPos(overlay_state.window.target_window, hwnd, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOCOPYBITS |
                         SWP_ASYNCWINDOWPOS);
      }
      return 0;
    }

    case WM_DESTROY:
      overlay_state.window.is_visible = false;
      return 0;
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}

// 设置全局状态指针（在初始化时调用）
void set_global_app_state(Core::State::AppState* state) { g_app_state = state; }

}  // namespace Features::Overlay::Utils
