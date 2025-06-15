import std;
import App;
import Utils.Logger;
import Vendor.Windows;

// 现代C++入口点
auto __stdcall wWinMain([[maybe_unused]] Vendor::Windows::HINSTANCE hInstance,
                        [[maybe_unused]] Vendor::Windows::HINSTANCE hPrevInstance,
                        [[maybe_unused]] Vendor::Windows::LPWSTR lpCmdLine,
                        [[maybe_unused]] int nCmdShow) -> int {
  // 使用 RAII 和现代C++特性
  try {
    auto app = std::make_unique<Application>();

    if (!app->Initialize(hInstance)) {
      Vendor::Windows::MessageBoxW(nullptr, L"Failed to initialize application", L"Error",
                                   Vendor::Windows::MB_ICONERROR_t);
      return -1;
    }

    return app->Run();

  } catch (const std::exception& e) {
    Logger().error("Unhandled exception: {}", e.what());
    Vendor::Windows::MessageBoxA(nullptr, e.what(), "Fatal Error", Vendor::Windows::MB_ICONERROR_t);
    return -1;
  }
}
