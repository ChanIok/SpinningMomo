#include "window_utils.hpp"
#include <wincodec.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

// 静态成员定义
std::unique_ptr<WindowCapturer> WindowUtils::s_capturer = std::make_unique<WindowCapturer>();
Microsoft::WRL::ComPtr<ID3D11Device> WindowUtils::s_device;
Microsoft::WRL::ComPtr<ID3D11DeviceContext> WindowUtils::s_context;
winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice WindowUtils::s_winrtDevice{ nullptr };

// 匿名命名空间：内部实现，只在当前文件可见
namespace {
    // 创建 Windows Runtime Direct3D 设备
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice CreateDirect3DDevice(ID3D11Device* d3dDevice) {
        // 获取DXGI设备接口
        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        HRESULT hr = d3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
        if (FAILED(hr)) return nullptr;
        
        // 创建WinRT设备
        winrt::com_ptr<::IInspectable> device;
        hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), device.put());
        if (FAILED(hr)) return nullptr;
        
        return device.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
    }

    // 创建窗口捕获项
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem CreateCaptureItemForWindow(HWND hwnd) {
        // 获取工厂接口
        auto factory = winrt::get_activation_factory<
            winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
            IGraphicsCaptureItemInterop>();
        
        // 创建捕获项
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem captureItem = { nullptr };
        HRESULT hr = factory->CreateForWindow(
            hwnd,
            winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
            reinterpret_cast<void**>(winrt::put_abi(captureItem)));
        
        if (FAILED(hr)) return nullptr;
        return captureItem;
    }

    // RAII 封装：自动管理 COM 初始化/清理
    class ComInitializer {
    public:
        ComInitializer() {
            APTTYPE aptType;
            APTTYPEQUALIFIER aptQualifier;
            if (SUCCEEDED(CoGetApartmentType(&aptType, &aptQualifier))) {
                // COM已经初始化，不需要再次初始化
                initialized_ = true;
                should_uninitialize_ = false;
            } else {
                // COM未初始化，尝试初始化
                initialized_ = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
                should_uninitialize_ = initialized_;
            }
        }
        ~ComInitializer() { 
            if (initialized_ && should_uninitialize_) {
                CoUninitialize(); 
            }
        }
        bool isInitialized() const { return initialized_; }
    private:
        bool initialized_;
        bool should_uninitialize_;
    };

    // RAII 封装：自动管理截图会话
    class CaptureSession {
    public:
        CaptureSession(winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool& pool,
                      winrt::Windows::Graphics::Capture::GraphicsCaptureItem& item,
                      winrt::Windows::Graphics::Capture::GraphicsCaptureSession& session)
            : session_(session) {
            if (session_) session_.StartCapture();
        }
        ~CaptureSession() { if (session_) session_.Close(); }
        operator bool() const { return session_ != nullptr; }
    private:
        winrt::Windows::Graphics::Capture::GraphicsCaptureSession session_;
    };

    // RAII 包装器：管理暂存纹理的映射状态
    class StagingTextureMapper {
    public:
        StagingTextureMapper(ID3D11DeviceContext* context, ID3D11Texture2D* texture)
            : m_context(context), m_texture(texture), m_mapped{} {
            m_success = SUCCEEDED(context->Map(texture, 0, D3D11_MAP_READ, 0, &m_mapped));
        }
        
        ~StagingTextureMapper() {
            if (m_success && m_context && m_texture) {
                m_context->Unmap(m_texture, 0);
            }
        }

        bool IsValid() const { return m_success; }
        const D3D11_MAPPED_SUBRESOURCE& GetMapped() const { return m_mapped; }

    private:
        ID3D11DeviceContext* m_context;
        ID3D11Texture2D* m_texture;
        D3D11_MAPPED_SUBRESOURCE m_mapped;
        bool m_success;
    };

    // RAII 包装器：管理 WIC 工厂
    class WICFactory {
    public:
        WICFactory() : m_factory(nullptr) {
            CoCreateInstance(
                CLSID_WICImagingFactory2,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&m_factory)
            );
        }

        IWICImagingFactory2* Get() const { return m_factory.Get(); }
        bool IsValid() const { return m_factory != nullptr; }

    private:
        Microsoft::WRL::ComPtr<IWICImagingFactory2> m_factory;
    };

    // RAII 包装器：管理 WIC 编码过程
    class WICImageEncoder {
    public:
        WICImageEncoder(const std::wstring& filePath) : m_success(false) {
            // 创建 WIC 工厂
            if (FAILED(CoCreateInstance(CLSID_WICImagingFactory2, nullptr,
                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_factory)))) {
                return;
            }

            // 创建编码器
            if (FAILED(m_factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &m_encoder))) {
                return;
            }

            // 创建流
            if (FAILED(m_factory->CreateStream(&m_stream))) {
                return;
            }

            // 初始化流
            if (FAILED(m_stream->InitializeFromFilename(filePath.c_str(), GENERIC_WRITE))) {
                return;
            }

            // 初始化编码器
            if (FAILED(m_encoder->Initialize(m_stream.Get(), WICBitmapEncoderNoCache))) {
                return;
            }

            // 创建帧
            if (FAILED(m_encoder->CreateNewFrame(&m_frame, nullptr))) {
                return;
            }

            // 初始化帧
            if (FAILED(m_frame->Initialize(nullptr))) {
                return;
            }

            m_success = true;
        }

        bool IsValid() const { return m_success; }

        bool SetSize(UINT width, UINT height) {
            return SUCCEEDED(m_frame->SetSize(width, height));
        }

        bool SetPixelFormat() {
            WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
            return SUCCEEDED(m_frame->SetPixelFormat(&format));
        }

        bool WritePixels(UINT height, UINT stride, UINT bufferSize, BYTE* data) {
            return SUCCEEDED(m_frame->WritePixels(height, stride, bufferSize, data));
        }

        // 新增：直接从 IWICBitmapSource 写入
        bool WriteSource(IWICBitmapSource* bitmap) {
            if (!bitmap || !m_frame) return false;
            
            // 获取位图大小
            UINT width = 0, height = 0;
            HRESULT hr = bitmap->GetSize(&width, &height);
            if (FAILED(hr)) return false;

            // 设置帧大小
            hr = m_frame->SetSize(width, height);
            if (FAILED(hr)) return false;

            // 设置像素格式
            WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
            hr = m_frame->SetPixelFormat(&format);
            if (FAILED(hr)) return false;

            // 写入图像数据
            return SUCCEEDED(m_frame->WriteSource(bitmap, nullptr));
        }

        bool Commit() {
            if (FAILED(m_frame->Commit())) return false;
            return SUCCEEDED(m_encoder->Commit());
        }

    private:
        Microsoft::WRL::ComPtr<IWICImagingFactory2> m_factory;
        Microsoft::WRL::ComPtr<IWICBitmapEncoder> m_encoder;
        Microsoft::WRL::ComPtr<IWICStream> m_stream;
        Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> m_frame;
        bool m_success;
    };
}

// 确保D3D资源已初始化
bool WindowUtils::EnsureD3DResources() {
    if (s_device && s_context && s_winrtDevice) return true;

    // 创建D3D设备
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
        &s_device, nullptr, &s_context);
    if (FAILED(hr)) return false;

    // 创建WinRT D3D设备
    s_winrtDevice = CreateDirect3DDevice(s_device.Get());
    if (!s_winrtDevice) return false;

    return true;
}


// 窗口查找
HWND WindowUtils::FindTargetWindow(const std::wstring& configuredTitle) {
    HWND gameWindow = NULL;
    
    // 辅助函数：去除字符串右侧空格
    auto trimRight = [](const std::wstring& str) -> std::wstring {
        size_t end = str.find_last_not_of(L' ');
        return (end == std::wstring::npos) ? L"" : str.substr(0, end + 1);
    };
    
    // 辅助函数：比较窗口标题
    auto compareWindowTitle = [&trimRight](const std::wstring& title1, const std::wstring& title2) -> bool {
        return trimRight(title1) == trimRight(title2);
    };
    
    // 1. 如果有配置的标题，先尝试使用配置的标题查找
    if (!configuredTitle.empty()) {
        auto windows = GetWindows();
        for (const auto& window : windows) {
            if (compareWindowTitle(window.second, configuredTitle)) {
                gameWindow = window.first;
                break;
            }
        }
    }
    
    // 2. 如果找不到，尝试预设的游戏窗口标题
    if (!gameWindow) {
        // 先尝试查找中文标题
        gameWindow = FindWindow(NULL, TEXT("无限暖暖  "));
        if (!gameWindow) {
            // 如果找不到中文标题，尝试英文标题
            gameWindow = FindWindow(NULL, TEXT("Infinity Nikki  "));
        }
    }
    
    return gameWindow;
}

// 窗口操作
bool WindowUtils::ResizeWindow(HWND hwnd, int width, int height, bool taskbarLower) {
    if (!hwnd || !IsWindow(hwnd)) return false;

    // 获取窗口样式
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 如果是有边框窗口且需要超出屏幕尺寸，转换为无边框
    if ((style & WS_OVERLAPPEDWINDOW) && (width > screenWidth || height > screenHeight)) {
        style &= ~(WS_OVERLAPPEDWINDOW);
        style |= WS_POPUP;
        SetWindowLong(hwnd, GWL_STYLE, style);
    }

    // 调整窗口大小
    RECT rect = { 0, 0, width, height };
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);

    // 使用 rect 的 left 和 top 值来调整位置这些值通常是负数
    int totalWidth = rect.right - rect.left;
    int totalHeight = rect.bottom - rect.top;
    int borderOffsetX = rect.left;  // 左边框的偏移量（负值）
    int borderOffsetY = rect.top;   // 顶部边框的偏移量（负值）

    // 计算屏幕中心位置，考虑边框偏移
    int newLeft = (screenWidth - width) / 2 + borderOffsetX;
    int newTop = (screenHeight - height) / 2 + borderOffsetY;

    // 设置新的窗口大小和位置
    bool success = SetWindowPos(hwnd, NULL, newLeft, newTop, totalWidth, totalHeight, 
                              SWP_NOZORDER) != FALSE;

    // 如果窗口调整成功且需要置底任务栏，则执行置底操作
    if (success && taskbarLower) {
        if (HWND taskbar = FindWindow(TEXT("Shell_TrayWnd"), NULL)) {
            SetWindowPos(taskbar, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    return success;
}

// 获取窗口列表
std::vector<std::pair<HWND, std::wstring>> WindowUtils::GetWindows() {
    std::vector<std::pair<HWND, std::wstring>> windows;
    
    // 回调函数
    auto enumWindowsProc = [](HWND hwnd, LPARAM lParam) -> BOOL {
        TCHAR className[256];
        TCHAR windowText[256];
        
        if (!IsWindowVisible(hwnd)) return TRUE;
        if (!GetClassName(hwnd, className, 256)) return TRUE;
        if (!GetWindowText(hwnd, windowText, 256)) return TRUE;

        auto windows = reinterpret_cast<std::vector<std::pair<HWND, std::wstring>>*>(lParam);
        if (windowText[0] != '\0') {  // 只收集有标题的窗口
            std::pair<HWND, std::wstring> item(hwnd, windowText);
            windows->push_back(item);
        }

        return TRUE;
    };

    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&windows));
    return windows;
}

// 分辨率计算
WindowUtils::Resolution WindowUtils::CalculateResolution(UINT64 totalPixels, double ratio) {
    // ratio 是宽高比，例如 9/16 = 0.5625
    int width = static_cast<int>(sqrt(totalPixels * ratio));
    int height = static_cast<int>(width / ratio);
    
    // 微调以确保总像素数准确
    if (static_cast<UINT64>(width) * height < totalPixels) {
        width++;
    }
    
    return WindowUtils::Resolution(width, height);
}

// 根据屏幕分辨率计算分辨率
WindowUtils::Resolution WindowUtils::CalculateResolutionByScreen(double targetRatio) {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 方案1：使用屏幕宽度计算高度
    int height1 = static_cast<int>(screenWidth / targetRatio);
    
    // 方案2：使用屏幕高度计算宽度
    int width2 = static_cast<int>(screenHeight * targetRatio);
    
    // 选择不超出屏幕的方案
    if (width2 <= screenWidth) {
        // 如果基于高度计算的宽度不超出屏幕，使用方案2
        return WindowUtils::Resolution(width2, screenHeight);
    } else {
        // 否则使用方案1
        return WindowUtils::Resolution(screenWidth, height1);
    }
}

// 获取程序截图路径
std::wstring WindowUtils::GetScreenshotPath() {
    const wchar_t* SCREENSHOT_DIR = L"ScreenShot";
    
    // 首选：程序所在目录
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) != 0) {
        std::wstring fullPath(exePath);
        std::wstring exeDir = fullPath.substr(0, fullPath.find_last_of(L"\\/"));
        std::wstring primaryPath = exeDir + L"\\" + SCREENSHOT_DIR;
        
        // 尝试在程序目录创建文件夹
        if (CreateDirectoryW(primaryPath.c_str(), NULL) || 
            GetLastError() == ERROR_ALREADY_EXISTS) {
            return primaryPath;
        }
    }
    
    // 备选：当前目录
    std::wstring fallbackPath = L".\\" + std::wstring(SCREENSHOT_DIR);
    CreateDirectoryW(fallbackPath.c_str(), NULL);  // 尝试创建，忽略结果
    return fallbackPath;
}

// 获取游戏截图路径
std::wstring WindowUtils::GetGameScreenshotPath(HWND hwnd) {
    if (!hwnd) return L"";

    // 获取进程ID
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId == 0) return L"";

    // 打开进程
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!hProcess) return L"";

    // 获取进程路径
    wchar_t processPath[MAX_PATH];
    DWORD size = MAX_PATH;
    BOOL success = QueryFullProcessImageNameW(hProcess, 0, processPath, &size);
    CloseHandle(hProcess);
    
    if (!success) return L"";

    // 构建相册路径
    std::wstring path = processPath;
    size_t binPos = path.find(L"\\Binaries\\Win64");
    if (binPos != std::wstring::npos) {
        path = path.substr(0, binPos) + L"\\ScreenShot";
        if (PathFileExistsW(path.c_str())) {
            return path;
        }
    }

    return L"";
} 

// 捕获窗口截图
bool WindowUtils::CaptureWindow(HWND hwnd, std::function<void(Microsoft::WRL::ComPtr<ID3D11Texture2D>)> callback, const RECT* cropRegion) {
    if (!hwnd || !callback) return false;
    
    // 确保捕获器已初始化
    if (!s_capturer->Initialize(hwnd)) return false;

    // 如果提供了裁剪区域，则设置裁剪
    if (cropRegion) {
        s_capturer->SetCropRegion(*cropRegion);
    }

    // 设置回调并等待捕获
    bool result = s_capturer->CaptureScreenshot([callback, capturer = s_capturer.get()](ID3D11Texture2D* texture) {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> capturedTexture;
        OutputDebugString(L"CaptureWindow: QueryInterface called\n");
        // 捕获完成后停止捕获
        capturer->StopCapture();
        if (SUCCEEDED(texture->QueryInterface(capturedTexture.GetAddressOf()))) {
            callback(capturedTexture);
        }
    });

    // 如果成功设置回调，开始捕获
    if (result) {
        s_capturer->StartCapture();
    }
    
    return result;
}

// 保存帧缓冲为文件
bool WindowUtils::SaveFrameToFile(ID3D11Texture2D* texture, const std::wstring& filePath) {
    // 获取纹理描述
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);

    // 获取设备和上下文
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    texture->GetDevice(&device);
    if (!device) return false;

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);
    if (!context) return false;

    // 创建暂存纹理
    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.ArraySize = 1;
    stagingDesc.MipLevels = 1;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;
    if (FAILED(device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture))) {
        return false;
    }

    // 复制纹理数据
    context->CopyResource(stagingTexture.Get(), texture);

    // 创建 WIC 编码器并设置参数
    WICImageEncoder encoder(filePath);
    if (!encoder.IsValid()) return false;
    
    if (!encoder.SetSize(desc.Width, desc.Height)) return false;
    if (!encoder.SetPixelFormat()) return false;

    // 映射纹理并写入数据
    {
        StagingTextureMapper mapper(context.Get(), stagingTexture.Get());
        if (!mapper.IsValid()) return false;

        const auto& mapped = mapper.GetMapped();
        if (!encoder.WritePixels(desc.Height, mapped.RowPitch, mapped.RowPitch * desc.Height,
            static_cast<BYTE*>(mapped.pData))) {
            return false;
        }
    }

    // 提交更改
    return encoder.Commit();
}
