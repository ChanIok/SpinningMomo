#include "window_utils.hpp"
#include <wincodec.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

// 窗口查找
HWND WindowUtils::FindGameWindow() {
    // 先尝试查找中文标题
    HWND hwnd = FindWindow(NULL, TEXT("无限暖暖  "));
    if (hwnd) return hwnd;
    
    // 如果找不到中文标题，尝试英文标题
    return FindWindow(NULL, TEXT("Infinity Nikki  "));
}

std::wstring WindowUtils::TrimRight(const std::wstring& str) {
    size_t end = str.find_last_not_of(L' ');
    return (end == std::wstring::npos) ? L"" : str.substr(0, end + 1);
}

bool WindowUtils::CompareWindowTitle(const std::wstring& title1, const std::wstring& title2) {
    return TrimRight(title1) == TrimRight(title2);
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
                              SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;

    // 如果窗口调整成功且需要置底任务栏，则执行置底操作
    if (success && taskbarLower) {
        if (HWND taskbar = FindWindow(TEXT("Shell_TrayWnd"), NULL)) {
            SetWindowPos(taskbar, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    return success;
}

// 回调函数
BOOL CALLBACK WindowUtils::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
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
}

// 获取窗口列表
std::vector<std::pair<HWND, std::wstring>> WindowUtils::GetWindows() {
    std::vector<std::pair<HWND, std::wstring>> windows;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&windows));
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


// 匿名命名空间：内部实现，只在当前文件可见
namespace {
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
                      winrt::Windows::Graphics::Capture::GraphicsCaptureItem& item)
            : session_(pool.CreateCaptureSession(item)) {
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

// 捕获窗口截图并保存到文件
bool WindowUtils::CaptureWindow(HWND hwnd, const std::wstring& savePath) {
    if (!hwnd) return false;
    
    // 检查COM状态
    APTTYPE aptType;
    APTTYPEQUALIFIER aptQualifier;
    HRESULT hr = CoGetApartmentType(&aptType, &aptQualifier);
    bool needUninitialize = false;
    
    if (FAILED(hr)) {
        hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hr)) return false;
        needUninitialize = true;
    }

    // 创建 D3D 设备和上下文
    Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dContext;
    
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
        &d3dDevice, nullptr, &d3dContext);
    if (FAILED(hr)) {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    // 创建 WinRT D3D 设备
    auto d3dDeviceWinRT = CreateDirect3DDevice(d3dDevice.Get());
    if (!d3dDeviceWinRT) {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    // 获取窗口尺寸
    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    // 创建窗口捕获项
    auto captureItem = CreateCaptureItemForWindow(hwnd);
    if (!captureItem) {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    // 设置帧缓冲池
    winrt::Windows::Graphics::SizeInt32 size{
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top
    };

    // 创建帧池
    auto framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(
        d3dDeviceWinRT,
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        1, size);
    if (!framePool) {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    // 创建并启动捕获会话
    CaptureSession session(framePool, captureItem);
    if (!session) {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    // 获取捕获的帧
    auto frame = framePool.TryGetNextFrame();
    int retryCount = 0;
    while (!frame && retryCount < 3) {
        Sleep(100);
        frame = framePool.TryGetNextFrame();
        retryCount++;
    }
    
    if (!frame) {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    // 获取帧表面
    auto frameSurface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
    if (!frameSurface) {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    // 保存帧
    bool success = SaveFrameToFile(frameSurface.Get(), savePath);

    // 清理资源
    framePool.Close();
    
    if (needUninitialize) {
        CoUninitialize();
    }

    return success;
}

// 创建 Windows Runtime Direct3D 设备
winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice WindowUtils::CreateDirect3DDevice(ID3D11Device* d3dDevice) {
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
winrt::Windows::Graphics::Capture::GraphicsCaptureItem WindowUtils::CreateCaptureItemForWindow(HWND hwnd) {
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