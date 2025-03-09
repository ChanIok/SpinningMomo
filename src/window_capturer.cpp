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
    if (m_cleanupTimer.IsRunning()) {
        m_cleanupTimer.Cancel();
    }
    Cleanup();
}

// 初始化捕获器
bool WindowCapturer::Initialize(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        LOG_ERROR("Invalid window handle");
        return false;
    }
    
    if (m_hwnd != hwnd) {
        m_hwnd = hwnd;
        // 新窗口句柄，重置尺寸记录
        m_targetWidth = 0;
        m_targetHeight = 0;
    }
    
    // 检查窗口尺寸是否变化
    bool sizeChanged = CheckWindowSizeChanged();
    
    // 确保D3D资源已初始化
    if (!EnsureD3DResources()) {
        LOG_ERROR("Failed to initialize D3D resources");
        return false;
    }

    // 如果尺寸变化，需要重新创建
    if (sizeChanged && m_isCapturing.load()) {
        // 如果正在捕获，先停止
        LOG_DEBUG("Window size changed, stopping capture");
        m_isCapturing.store(false);
        if (m_framePool) {
            m_framePool.FrameArrived(m_frameArrivedToken);
            m_framePool.Close();
            m_framePool = nullptr;
        }
        if (m_captureSession) {
            m_captureSession.Close();
            m_captureSession = nullptr;
        }
        m_captureItem = nullptr;
    } else if (m_isCapturing.load()) {
        LOG_DEBUG("Already capturing, skipping initialization");
        return true;
    }

    // 创建新的捕获会话
    if (!CreateCaptureSession()) {
        LOG_ERROR("Failed to create capture session");
        Cleanup();
        return false;
    }
    LOG_DEBUG("Capture session created");
    return true;
}

// 添加回调到队列
void WindowCapturer::AddCaptureCallback(std::function<void(ID3D11Texture2D*)> callback) {
    if (!callback) return;
    
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_callbackQueue.push(std::move(callback));
}

// 简化API：设置回调并立即开始捕获
bool WindowCapturer::CaptureOneFrame(HWND hwnd, std::function<void(ID3D11Texture2D*)> callback) {
    if (!callback) {
        LOG_ERROR("Callback is null");
        return false;
    }
    
    // 初始化捕获器
    if (!Initialize(hwnd)) {
        LOG_ERROR("Failed to initialize capturer");
        return false;
    }
    
    // 添加回调到队列
    AddCaptureCallback(std::move(callback));
    
    // 开始捕获
    return StartCapture();
}

// 开始捕获
bool WindowCapturer::StartCapture() {
    if (m_isCapturing.load()) return true; // 已经在捕获中
    
    if (m_captureSession) {
        LOG_DEBUG("Starting capture process");
        m_captureSession.StartCapture();
        m_isCapturing.store(true);
        return true;
    }
    
    LOG_ERROR("Capture session is null");
    return false;
}

// 清理资源
void WindowCapturer::Cleanup() {
    if (!m_isCapturing.load()) {    // 防止重入
        return;
    }

    m_isCapturing.store(false);

    if (m_framePool) {
        m_framePool.FrameArrived(m_frameArrivedToken);
        m_framePool.Close();
        m_framePool = nullptr;
    }
    if (m_captureSession) {
        m_captureSession.Close();
        m_captureSession = nullptr;
    }
    m_captureItem = nullptr;
    m_hwnd = nullptr;
    
    // 清空回调队列
    std::lock_guard<std::mutex> lock(m_queueMutex);
    std::queue<std::function<void(ID3D11Texture2D*)>> emptyQueue;
    std::swap(m_callbackQueue, emptyQueue);

    // 清理D3D资源
    WindowUtils::CleanupCaptureResources();
}

bool WindowCapturer::EnsureD3DResources() {
    return WindowUtils::EnsureD3DResources();
}

// 检查窗口尺寸是否变化
bool WindowCapturer::CheckWindowSizeChanged() {
    if (!m_hwnd || !IsWindow(m_hwnd)) {
        return false;
    }
    
    RECT windowRect;
    if (!GetWindowRect(m_hwnd, &windowRect)) {
        return false;
    }
    
    int newWidth = static_cast<int>(windowRect.right - windowRect.left);
    int newHeight = static_cast<int>(windowRect.bottom - windowRect.top);
    
    // 检查尺寸是否变化
    if (newWidth != m_targetWidth || newHeight != m_targetHeight) {
        LOG_DEBUG("Window size changed: " + std::to_string(m_targetWidth) + "x" + std::to_string(m_targetHeight) + 
                 " -> " + std::to_string(newWidth) + "x" + std::to_string(newHeight));
        m_targetWidth = newWidth;
        m_targetHeight = newHeight;
        return true; // 尺寸已变化
    }
    
    return false; // 尺寸未变化
}

bool WindowCapturer::CreateCaptureSession() {
    // 确保已获取窗口尺寸
    if (m_targetWidth == 0 || m_targetHeight == 0) {
        RECT windowRect;
        if (!GetWindowRect(m_hwnd, &windowRect)) return false;
        m_targetWidth = static_cast<int>(windowRect.right - windowRect.left);
        m_targetHeight = static_cast<int>(windowRect.bottom - windowRect.top);
    }

    // 创建捕获项
    m_captureItem = CreateCaptureItemForWindow(m_hwnd);
    if (!m_captureItem) return false;

    // 创建帧池，使用已保存的尺寸
    m_framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(
        WindowUtils::GetWinRTDevice(),
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        1,
        { m_targetWidth, m_targetHeight }
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
        m_needHideCursor = true;
        WindowUtils::HideCursor();  // 使用WindowUtils提供的鼠标隐藏功能
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

void WindowCapturer::ProcessFrameArrived(ID3D11Texture2D* texture) {
    if (!m_isCapturing.load() || !texture) return;
    
    std::function<void(ID3D11Texture2D*)> callback;
    bool hasCallback = false;
    
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (!m_callbackQueue.empty()) {
            callback = std::move(m_callbackQueue.front());
            m_callbackQueue.pop();
            hasCallback = true;
            
            // 队列有回调，取消清理定时器
            if (m_cleanupTimer.IsRunning()) {
                m_cleanupTimer.Cancel();
                LOG_DEBUG("Cleanup timer cancelled due to new callbacks");
            }
        }
    }
    
    if (hasCallback && callback) {
        callback(texture);
    }

    // 兼容性隐藏鼠标后，立即停止捕获
    if (m_needHideCursor) {
        WindowUtils::ShowCursor();
        Cleanup();
        return;
    }

    // 如果队列为空且定时器未激活，则设置定时器
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_callbackQueue.empty() && !m_cleanupTimer.IsRunning()) {
            LOG_DEBUG("Queue empty, starting cleanup timer");
            
            m_cleanupTimer.SetTimer(CLEANUP_TIMEOUT, [this]() {
                LOG_DEBUG("Cleanup timer triggered, stopping capture");
                Cleanup();
            });
        }
    }
}