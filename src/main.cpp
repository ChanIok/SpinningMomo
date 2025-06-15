#include <windows.h>

import std;
import Core.ConfigManager;
import Core.Constants;
import Utils.Logger;

// 主应用程序类 - 现代C++重构版
class SpinningMomoApp {
 public:
  SpinningMomoApp() = default;
  ~SpinningMomoApp() = default;

  // 现代C++风格的初始化
  [[nodiscard]] auto Initialize(HINSTANCE hInstance) -> bool {
    try {
      LogSystemInfo();

      // 1. 使用现代C++20模块初始化配置管理器
      m_config_manager = std::make_unique<Core::Config::ConfigManager>();

      // 2. 初始化配置管理器 - 使用 std::expected 处理错误
      if (auto init_result = m_config_manager->Initialize(); !init_result) {
        Logger().error("Failed to initialize config manager: {}", init_result.error());
        return false;
      }

      // 3. 加载所有配置 - 现代错误处理
      if (auto load_result = m_config_manager->LoadAllConfigs(); !load_result) {
        Logger().error("Failed to load configurations: {}", load_result.error());
        return false;
      }

      // 4. 获取配置 - 使用现代C++的 std::expected
      if (auto hotkey_config = m_config_manager->GetHotkeyConfig(); hotkey_config) {
        m_hotkey = hotkey_config.value();
        Logger().info("Hotkey configured: modifiers={}, key={}", m_hotkey.modifiers, m_hotkey.key);
      } else {
        Logger().warn("Failed to get hotkey config: {}", hotkey_config.error());
      }

      if (auto window_config = m_config_manager->GetWindowConfig(); window_config) {
        m_window_title = window_config.value().title;
        if (!m_window_title.empty()) {
          Logger().info("Window title configured: {}", ToUtf8(m_window_title));
        }
      }

      if (auto language_config = m_config_manager->GetLanguageConfig(); language_config) {
        m_current_language = language_config.value().current_language;
        Logger().info("Language configured: {}", ToUtf8(m_current_language));
      }

      // 5. 获取宽高比和分辨率配置 - 现代C++风格
      InitializeRatiosAndResolutions();

      // 6. 其他初始化...
      if (!CreateMainWindow(hInstance)) {
        Logger().error("Failed to create main window");
        return false;
      }

      Logger().info("Application initialized successfully");
      return true;

    } catch (const std::exception& e) {
      Logger().error("Exception during initialization: {}", e.what());
      return false;
    }
  }

  [[nodiscard]] auto Run() -> int {
    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
  }

 private:
  // 现代C++配置管理器
  std::unique_ptr<Core::Config::ConfigManager> m_config_manager;

  // 配置数据
  Core::Config::HotkeyConfig m_hotkey{};
  std::wstring m_window_title;
  std::wstring m_current_language;

  // 主窗口
  HWND m_main_window = nullptr;

  // 现代C++风格的辅助方法
  void InitializeRatiosAndResolutions() {
    // 获取本地化字符串（假设存在）
    // const auto& strings = GetLocalizedStrings();

    // 使用现代C++的配置加载结果
    // if (auto ratio_result = m_config_manager->GetAspectRatios(strings); ratio_result.success) {
    //     m_ratios = std::move(ratio_result.ratios);
    //     LogInfo(std::format("Loaded {} aspect ratios", m_ratios.size()));
    // } else {
    //     LogError(std::format("Failed to load aspect ratios: {}",
    //     ToUtf8(ratio_result.error_details)));
    // }
  }

  [[nodiscard]] auto CreateMainWindow(HINSTANCE hInstance) -> bool {
    // 窗口创建逻辑
    const auto class_name = L"SpinningMomoModernApp";

    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WindowProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = class_name;

    if (!RegisterClassExW(&wcex)) {
      Logger().error("Failed to register window class");
      return false;
    }

    const auto window_title = m_window_title.empty() ? L"SpinningMomo" : m_window_title.c_str();

    m_main_window =
        CreateWindowW(class_name, window_title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                      800, 600, nullptr, nullptr, hInstance, static_cast<LPVOID>(this));

    if (!m_main_window) {
      Logger().error("Failed to create main window");
      return false;
    }

    // 显示窗口（用于测试）
    ShowWindow(m_main_window, SW_SHOWNORMAL);
    UpdateWindow(m_main_window);

    Logger().info("Main window created and shown");
    return true;
  }

  void LogSystemInfo() {
    OSVERSIONINFOEXW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);

    if (GetVersionExW(reinterpret_cast<LPOSVERSIONINFOW>(&osvi))) {
      Logger().info("OS Version: {}.{}.{}", osvi.dwMajorVersion, osvi.dwMinorVersion,
                    osvi.dwBuildNumber);
    }
  }

  // 字符串转换辅助函数
  [[nodiscard]] auto ToUtf8(const std::wstring& wide_str) const -> std::string {
    if (wide_str.empty()) return {};

    const auto size_needed =
        WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), static_cast<int>(wide_str.size()),
                            nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return {};

    std::string result(size_needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), static_cast<int>(wide_str.size()),
                        result.data(), size_needed, nullptr, nullptr);
    return result;
  }

  // 现代窗口过程
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<SpinningMomoApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
      case WM_CREATE: {
        auto* create_struct = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
        return 0;
      }
      case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
      default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
  }
};

// 现代C++入口点
int WINAPI wWinMain([[maybe_unused]] HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
                    [[maybe_unused]] LPWSTR lpCmdLine, [[maybe_unused]] int nCmdShow) {
  // 使用 RAII 和现代C++特性
  try {
    auto app = std::make_unique<SpinningMomoApp>();

    if (!app->Initialize(hInstance)) {
      MessageBoxW(nullptr, L"Failed to initialize application", L"Error", MB_ICONERROR);
      return -1;
    }

    return app->Run();

  } catch (const std::exception& e) {
    Logger().error("Unhandled exception: {}", e.what());
    MessageBoxA(nullptr, e.what(), "Fatal Error", MB_ICONERROR);
    return -1;
  }
}
