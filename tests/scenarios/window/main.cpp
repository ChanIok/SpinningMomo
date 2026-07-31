#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/shellapi.hpp"

namespace {

struct Options {
  std::wstring title = L"SpinningMomo Scenario Target";
  std::filesystem::path ready_path;
  int client_width = 640;
  int client_height = 360;
};

struct WindowState {
  std::uint32_t frame = 0;
};

auto parse_positive_int(std::wstring_view value, int fallback) -> int {
  if (value.empty()) {
    return fallback;
  }

  wchar_t* end = nullptr;
  const auto parsed = std::wcstol(value.data(), &end, 10);
  if (end == value.data() || *end != L'\0' || parsed <= 0 || parsed > 4096) {
    return fallback;
  }

  return static_cast<int>(parsed);
}

auto parse_options() -> std::optional<Options> {
  int argument_count = 0;
  auto* arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
  if (!arguments) {
    return std::nullopt;
  }

  Options options;
  for (int index = 1; index < argument_count; ++index) {
    const std::wstring_view argument(arguments[index]);
    if (argument == L"--title" && index + 1 < argument_count) {
      options.title = arguments[++index];
    } else if (argument == L"--ready" && index + 1 < argument_count) {
      options.ready_path = arguments[++index];
    } else if (argument == L"--width" && index + 1 < argument_count) {
      options.client_width = parse_positive_int(arguments[++index], options.client_width);
    } else if (argument == L"--height" && index + 1 < argument_count) {
      options.client_height = parse_positive_int(arguments[++index], options.client_height);
    }
  }

  LocalFree(arguments);
  if (options.title.empty()) {
    return std::nullopt;
  }
  return options;
}

auto write_ready_file(const std::filesystem::path& path, HWND hwnd) -> bool {
  if (path.empty()) {
    return true;
  }

  if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd) || GetWindowTextLengthW(hwnd) <= 0) {
    return false;
  }

  std::error_code error;
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path(), error);
  }

  if (error) {
    return false;
  }

  std::ofstream ready_file(path, std::ios::binary | std::ios::trunc);
  if (ready_file) {
    ready_file << "ready\n";
  }
  return ready_file.good();
}

auto paint_color_block(HDC device_context, const RECT& rectangle, COLORREF color) -> void {
  auto brush = CreateSolidBrush(color);
  if (!brush) {
    return;
  }

  FillRect(device_context, &rectangle, brush);
  DeleteObject(brush);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  auto* state = reinterpret_cast<WindowState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  switch (message) {
    case WM_NCCREATE: {
      auto* create_struct = reinterpret_cast<const CREATESTRUCTW*>(lparam);
      state = static_cast<WindowState*>(create_struct->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
      break;
    }

    case WM_PAINT: {
      PAINTSTRUCT paint{};
      auto device_context = BeginPaint(hwnd, &paint);
      RECT client_rect{};
      GetClientRect(hwnd, &client_rect);

      const int middle_x = (client_rect.left + client_rect.right) / 2;
      const int middle_y = (client_rect.top + client_rect.bottom) / 2;
      const RECT top_left{client_rect.left, client_rect.top, middle_x, middle_y};
      const RECT top_right{middle_x, client_rect.top, client_rect.right, middle_y};
      const RECT bottom_left{client_rect.left, middle_y, middle_x, client_rect.bottom};
      const RECT bottom_right{middle_x, middle_y, client_rect.right, client_rect.bottom};

      paint_color_block(device_context, top_left, RGB(220, 70, 70));
      paint_color_block(device_context, top_right, RGB(70, 150, 230));
      paint_color_block(device_context, bottom_left, RGB(80, 190, 110));
      paint_color_block(device_context, bottom_right, RGB(235, 185, 65));

      if (state) {
        SetBkMode(device_context, TRANSPARENT);
        SetTextColor(device_context, RGB(255, 255, 255));
        const auto text =
            std::wstring(L"SpinningMomo scenario target\nframe: ") + std::to_wstring(state->frame);
        auto text_rect = client_rect;
        DrawTextW(device_context, text.c_str(), -1, &text_rect,
                  DT_CENTER | DT_VCENTER | DT_WORDBREAK);
      }

      EndPaint(hwnd, &paint);
      return 0;
    }

    case WM_ERASEBKGND:
      return 1;

    case WM_TIMER:
      if (state) {
        ++state->frame;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;

    case WM_DESTROY:
      KillTimer(hwnd, 1);
      PostQuitMessage(0);
      return 0;

    default:
      break;
  }

  return DefWindowProcW(hwnd, message, wparam, lparam);
}

auto run_window(const Options& options) -> int {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  const auto instance = GetModuleHandleW(nullptr);
  constexpr wchar_t kWindowClassName[] = L"SpinningMomoScenarioWindowClass";

  WNDCLASSW window_class{
      .style = CS_HREDRAW | CS_VREDRAW,
      .lpfnWndProc = window_proc,
      .hInstance = instance,
      .hCursor = LoadCursorW(nullptr, IDC_ARROW),
      .lpszClassName = kWindowClassName,
  };
  if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return 1;
  }

  RECT window_rect{0, 0, options.client_width, options.client_height};
  if (!AdjustWindowRectEx(&window_rect, WS_OVERLAPPEDWINDOW, FALSE, 0)) {
    return 1;
  }

  WindowState state{};
  const int window_width = window_rect.right - window_rect.left;
  const int window_height = window_rect.bottom - window_rect.top;
  const auto hwnd = CreateWindowExW(WS_EX_APPWINDOW, kWindowClassName, options.title.c_str(),
                                    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, window_width,
                                    window_height, nullptr, nullptr, instance, &state);
  if (!hwnd) {
    return 1;
  }

  ShowWindow(hwnd, SW_SHOWNORMAL);
  UpdateWindow(hwnd);
  SetTimer(hwnd, 1, 100, nullptr);
  if (!write_ready_file(options.ready_path, hwnd)) {
    DestroyWindow(hwnd);
    return 1;
  }

  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  if (!options.ready_path.empty()) {
    std::error_code error;
    std::filesystem::remove(options.ready_path, error);
  }
  return static_cast<int>(message.wParam);
}

}  // namespace

auto __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) -> int {
  const auto options = parse_options();
  return options ? run_window(*options) : 1;
}
