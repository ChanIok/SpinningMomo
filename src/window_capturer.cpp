#include "window_capturer.hpp"
#include "window_utils.hpp"
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <queue>
#include <mutex>
#include "logger.hpp"

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

    // 创建帧池
    m_framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(
        WindowUtils::GetWinRTDevice(),
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        1,
        { windowRect.right - windowRect.left, windowRect.bottom - windowRect.top }
    );
    if (!m_framePool) return false;

    // 设置帧到达回调
    m_frameArrivedToken = m_framePool.FrameArrived([this](Direct3D11CaptureFramePool const& sender, 
        winrt::Windows::Foundation::IInspectable const&) {
        auto frame = sender.TryGetNextFrame();
        if (frame) {
            auto surface = frame.Surface();
            if (surface) {
                auto texture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(surface);
                if (texture) {
                    ProcessFrameArrived(texture.get());
                }
            }
        }
    });

    // 创建捕获会话
    m_captureSession = m_framePool.CreateCaptureSession(m_captureItem);
    if (!m_captureSession) return false;

    if (winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
        winrt::name_of<winrt::Windows::Graphics::Capture::GraphicsCaptureSession>(),
        L"IsCursorCaptureEnabled")) 
    {
        m_captureSession.IsCursorCaptureEnabled(false);
    } else {
        LOG_INFO("Cursor capture setting not available on this Windows version");
    }
    
    if (winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
        winrt::name_of<winrt::Windows::Graphics::Capture::GraphicsCaptureSession>(),
        L"IsBorderRequired")) 
    {
        m_captureSession.IsBorderRequired(false);
    } else {
        LOG_INFO("Border requirement setting not available on this Windows version");
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