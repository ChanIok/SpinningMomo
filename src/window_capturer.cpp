#include "window_capturer.hpp"
#include "window_utils.hpp"
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <queue>
#include <mutex>

using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

// 匿名命名空间：内部实现，只在当前文件可见
namespace {
    // 创建窗口捕获项
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem CreateCaptureItemForWindow(HWND hwnd) {
        auto factory = winrt::get_activation_factory<
            winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
            IGraphicsCaptureItemInterop>();
        
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem captureItem{ nullptr };
        HRESULT hr = factory->CreateForWindow(
            hwnd,
            winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
            reinterpret_cast<void**>(winrt::put_abi(captureItem)));
        
        if (FAILED(hr)) return nullptr;
        return captureItem;
    }

    template<typename T>
    winrt::com_ptr<T> GetDXGIInterfaceFromObject(winrt::Windows::Foundation::IInspectable const& object) {
        auto access = object.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        winrt::com_ptr<T> result;
        winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(&result)));
        return result;
    }
}

WindowCapturer::WindowCapturer() {
    // 确保在构造函数中不初始化 COM，因为它应该在使用的线程中初始化
}

WindowCapturer::~WindowCapturer() {
    Cleanup();
}

bool WindowCapturer::Initialize(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    
    m_hwnd = hwnd;

    // 确保D3D资源已初始化
    if (!EnsureD3DResources()) return false;

    // 创建捕获会话
    if (!CreateCaptureSession()) return false;

    m_isInitialized = true;
    return true;
}

void WindowCapturer::Cleanup() {
    if (m_framePool) {
        m_framePool.Close();
        m_framePool = nullptr;
    }
    if (m_captureSession) {
        m_captureSession.Close();
        m_captureSession = nullptr;
    }
    m_captureItem = nullptr;
    m_isInitialized = false;
    m_isCapturing = false;
}

bool WindowCapturer::EnsureD3DResources() {
    return WindowUtils::EnsureD3DResources();
}

bool WindowCapturer::CreateCaptureSession() {
    // 获取窗口尺寸
    RECT windowRect;
    if (!GetWindowRect(m_hwnd, &windowRect)) return false;

    // 创建捕获项
    m_captureItem = CreateCaptureItemForWindow(m_hwnd);
    if (!m_captureItem) return false;

    // 确定帧池大小（总是使用完整窗口大小）
    int poolWidth = windowRect.right - windowRect.left;
    int poolHeight = windowRect.bottom - windowRect.top;
    
    // 创建帧池
    m_framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(
        WindowUtils::GetWinRTDevice(),
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        2,  // 使用2个缓冲区
        { poolWidth, poolHeight }
    );
    if (!m_framePool) return false;

    // 设置帧到达回调
    m_frameArrivedToken = m_framePool.FrameArrived([this, poolWidth, poolHeight](Direct3D11CaptureFramePool const& sender, 
        winrt::Windows::Foundation::IInspectable const&) {
        auto frame = sender.TryGetNextFrame();
        if (!frame) return;

        auto surface = frame.Surface();
        if (!surface) return;

        auto texture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(surface);
        if (!texture) return;

        // 如果不需要裁剪，直接使用原始纹理
        if (!m_needsCropping) {
            ProcessFrameArrived(texture.get());
            return;
        }

        // 验证裁剪区域是否有效
        if (m_cropRegion.left < 0 || m_cropRegion.top < 0 ||
            m_cropRegion.right > poolWidth || m_cropRegion.bottom > poolHeight) {
            OutputDebugStringA("Invalid crop region, using full texture\n");
            ProcessFrameArrived(texture.get());
            return;
        }
        
        // 创建裁剪纹理描述
        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);
        desc.Width = m_cropRegion.right - m_cropRegion.left;
        desc.Height = m_cropRegion.bottom - m_cropRegion.top;

        // 获取设备并创建裁剪纹理
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        texture->GetDevice(&device);
        if (!device) {
            ProcessFrameArrived(texture.get());
            return;
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> croppedTexture;
        HRESULT hr = device->CreateTexture2D(&desc, nullptr, &croppedTexture);
        if (FAILED(hr)) {
            ProcessFrameArrived(texture.get());
            return;
        }

        // 获取上下文并执行裁剪
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        device->GetImmediateContext(&context);
        if (!context) {
            ProcessFrameArrived(texture.get());
            return;
        }

        // 设置复制区域
        D3D11_BOX sourceBox = {
            static_cast<UINT>(m_cropRegion.left),
            static_cast<UINT>(m_cropRegion.top),
            0,  // front
            static_cast<UINT>(m_cropRegion.right),
            static_cast<UINT>(m_cropRegion.bottom),
            1   // back
        };

        // 复制指定区域
        context->CopySubresourceRegion(
            croppedTexture.Get(), 0, 0, 0, 0,
            texture.get(), 0, &sourceBox
        );

        ProcessFrameArrived(croppedTexture.Get());
    });

    // 创建捕获会话
    m_captureSession = m_framePool.CreateCaptureSession(m_captureItem);
    if (!m_captureSession) return false;

    // 禁用光标捕获
    m_captureSession.IsCursorCaptureEnabled(false);

    // 尝试禁用边框
    try {
        auto session3 = m_captureSession.try_as<winrt::Windows::Graphics::Capture::IGraphicsCaptureSession3>();
        if (session3) {
            session3.IsBorderRequired(false);
        }
    }
    catch (...) {
        // 忽略错误
    }

    return true;
}

void WindowCapturer::StartCapture() {
    if (m_captureSession && !m_isCapturing) {
        m_captureSession.StartCapture();
        m_isCapturing = true;
    }
}

void WindowCapturer::StopCapture() {
    if (!m_isCapturing) {
        return;
    }

    Cleanup();
}

bool WindowCapturer::CaptureScreenshot(std::function<void(ID3D11Texture2D*)> callback) {
    if (!m_isInitialized || !callback) return false;
    
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_callbackQueue.push(callback);
    }
    
    return true;
}

void WindowCapturer::ProcessFrameArrived(ID3D11Texture2D* texture) {
    std::function<void(ID3D11Texture2D*)> callback;
    wchar_t threadIdStr[32];
    _itow_s(GetCurrentThreadId(), threadIdStr, 32, 10);
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (!m_callbackQueue.empty()) {
            callback = std::move(m_callbackQueue.front());
            m_callbackQueue.pop();
            
            // 立即停止捕获，因为我们只需要一帧
            m_isCapturing = false;
            if (m_captureSession) {
                m_captureSession.Close();
            }
        }
    }
    
    if (callback) {
        callback(texture);
    }
}