#include "parameter_tracker.hpp"
#include "window_utils.hpp"
#include "parameter_ocr.hpp"
#include <thread>
#include <fstream>

// 静态成员定义
HHOOK ParameterTracker::s_mouseHook = NULL;
HWND ParameterTracker::s_targetWindow = NULL;
std::shared_ptr<ParameterTracker::CaptureSequence> ParameterTracker::s_currentSequence;
std::mutex ParameterTracker::s_sequenceMutex;
std::unique_ptr<std::thread> ParameterTracker::s_workerThread;
HWND ParameterTracker::s_workerWindow = NULL;
std::atomic<bool> ParameterTracker::s_threadRunning{false};
ParameterTracker::AllParameters ParameterTracker::s_currentParams;

// 初始化参数追踪器
bool ParameterTracker::Initialize(HWND targetWindow) {
    s_targetWindow = targetWindow;
    
    // 启动工作线程
    s_threadRunning = true;
    s_workerThread = std::make_unique<std::thread>(&WorkerThreadProc);

    return true;
}

// 清理参数追踪器
void ParameterTracker::Cleanup() {
    // 清理鼠标钩子
    if (s_mouseHook) {
        UnhookWindowsHookEx(s_mouseHook);
        s_mouseHook = NULL;
    }
    
    // 清理工作线程
    s_threadRunning = false;
    if (s_workerWindow) {
        PostMessage(s_workerWindow, WM_QUIT, 0, 0);
    }
    if (s_workerThread && s_workerThread->joinable()) {
        s_workerThread->join();
    }
    s_workerThread.reset();

    // 清理窗口和COM
    if (s_workerWindow) {
        DestroyWindow(s_workerWindow);
        s_workerWindow = NULL;
        UnregisterClass(WORKER_WINDOW_CLASS, GetModuleHandle(NULL));
        CoUninitialize();
    }

    s_targetWindow = NULL;
}

// 工作线程函数
void ParameterTracker::WorkerThreadProc() {
    // 初始化COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        s_threadRunning = false;
        return;
    }

    // 注册窗口类
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WorkerWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = WORKER_WINDOW_CLASS;
    
    RegisterClassEx(&wc);
    
    // 创建工作线程的消息窗口
    s_workerWindow = CreateWindowEx(
        0, WORKER_WINDOW_CLASS, L"CaptureWorker",
        WS_OVERLAPPED,
        0, 0, 0, 0,
        HWND_MESSAGE, nullptr, GetModuleHandle(NULL), nullptr
    );

    if (!s_workerWindow) {
        CoUninitialize();
        s_threadRunning = false;
        return;
    }

    // 安装鼠标钩子
    s_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, 
        GetModuleHandle(NULL), 0);
        
    if (!s_mouseHook) {
        DestroyWindow(s_workerWindow);
        s_workerWindow = NULL;
        UnregisterClass(WORKER_WINDOW_CLASS, GetModuleHandle(NULL));
        CoUninitialize();
        s_threadRunning = false;
        return;
    }

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 只设置线程状态
    s_threadRunning = false;
}

// 工作线程消息处理
LRESULT CALLBACK ParameterTracker::WorkerWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case Constants::WM_PARAMETER_START_CAPTURE: {
            std::lock_guard<std::mutex> lock(s_sequenceMutex);
            if (!s_currentSequence || !s_currentSequence->IsActive()) return 0;

            OutputDebugStringA("ParameterTracker::WorkerWndProc: WM_PARAMETER_START_CAPTURE\n");
            
            // 获取当前序列的裁剪区域
            RECT cropRegion = s_currentSequence->GetRegion();
            
            // 调用WindowUtils进行捕获
            WindowUtils::CaptureWindow(s_targetWindow, 
                [](Microsoft::WRL::ComPtr<ID3D11Texture2D> texture) {
                    OutputDebugStringA("ParameterTracker: Capture callback executed\n");
                    // TODO: 后续处理捕获的纹理
                }, &cropRegion);
            
            s_currentSequence->SetActive(false);
            return 0;
        }

        case WM_TIMER: {
            KillTimer(hwnd, wp);
            std::lock_guard<std::mutex> lock(s_sequenceMutex);
            if (s_currentSequence && s_currentSequence->IsActive()) {
                PostMessage(hwnd, Constants::WM_PARAMETER_START_CAPTURE, 0, 0);
            }
            return 0;
        }
        
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

// 开始新的捕获序列
void ParameterTracker::StartCaptureSequence(const RECT& region) {
    std::lock_guard<std::mutex> lock(s_sequenceMutex);
    
    // 如果当前有活动的序列且未完成，则忽略新的请求
    if (s_currentSequence && s_currentSequence->IsActive()) {
        return;
    }

    OutputDebugStringA("ParameterTracker::StartCaptureSequence: StartCaptureSequence\n");
    
    // 创建并启动新的捕获序列
    s_currentSequence = std::make_shared<CaptureSequence>(region);
    s_currentSequence->Start();
}

// 捕获序列启动
void ParameterTracker::CaptureSequence::Start() {
    if (m_isActive) return;
    
    m_isActive = true;
    m_captureCount = 0;
    
    // 发送开始捕获消息到工作线程
    if (s_workerWindow) {
        PostMessage(s_workerWindow, Constants::WM_PARAMETER_START_CAPTURE, 0, 0);
    }
}

// 捕获序列实现
ParameterTracker::CaptureSequence::CaptureSequence(const RECT& region)
    : m_region(region), m_captureCount(0) {
}

// 捕获序列析构
ParameterTracker::CaptureSequence::~CaptureSequence() {
    SetActive(false);
}



void ParameterTracker::CaptureSequence::ProcessCapture(Microsoft::WRL::ComPtr<IWICBitmapSource> bitmap) {
    if (!m_isActive) return;

    // 保存图片（用于调试）
    auto now = std::chrono::system_clock::now();
    std::wstring filename = L"Parameter_" + 
        std::to_wstring(std::chrono::system_clock::to_time_t(now)) + 
        L"_" + std::to_wstring(m_captureCount) + L".png";
    std::wstring savePath = WindowUtils::GetScreenshotPath() + L"\\" + filename;
    
    OutputDebugStringA("ParameterTracker::CaptureSequence::ProcessCapture ");
    
    m_captureCount++;
    
    // 检查是否完成所有捕获
    if (m_captureCount >= MAX_CAPTURES) {
        m_isActive = false;
    }
}

RECT ParameterTracker::CalculateParameterArea(const RECT& clientRect) {
    // 计算窗口比例
    double windowRatio = static_cast<double>(clientRect.right) / clientRect.bottom;
    int areaWidth, areaHeight;
    
    if (windowRatio >= 16.0/9.0) {
        // 宽幅比例下，区域高度与窗口高度的比例固定
        areaHeight = static_cast<int>(clientRect.bottom * AREA_HEIGHT_RATIO);
        areaWidth = static_cast<int>(areaHeight * AREA_ASPECT_RATIO);
    } else {
        // 非宽幅比例下，基于窗口宽度计算
        areaWidth = static_cast<int>(clientRect.right * AREA_WIDTH_RATIO);
        areaHeight = static_cast<int>(areaWidth / AREA_ASPECT_RATIO);
    }
    
    // 计算区域的顶部位置（确保区域底部对齐窗口底部）
    int areaTop = clientRect.bottom - areaHeight;
    
    return {
        0,                  // left
        areaTop,           // top
        areaWidth,         // right
        clientRect.bottom  // bottom
    };
}

LRESULT CALLBACK ParameterTracker::MouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && wParam == WM_LBUTTONDOWN) {
        MSLLHOOKSTRUCT* hookStruct = (MSLLHOOKSTRUCT*)lParam;
        POINT pt = hookStruct->pt;
        
        if (s_targetWindow && IsWindow(s_targetWindow) && IsInParameterArea(pt)) {
            // 获取窗口客户区大小
            RECT clientRect;
            GetClientRect(s_targetWindow, &clientRect);

            // 计算参数区域
            RECT paramArea = CalculateParameterArea(clientRect);
            
            // 开始新的捕获序列
            StartCaptureSequence(paramArea);
        }
    }
    return CallNextHookEx(s_mouseHook, code, wParam, lParam);
}

bool ParameterTracker::IsInParameterArea(const POINT& pt) {
    if (!s_targetWindow || !IsWindow(s_targetWindow)) return false;

    // 转换屏幕坐标到窗口坐标
    POINT clientPt = pt;
    if (!ScreenToClient(s_targetWindow, &clientPt)) return false;

    // 获取窗口客户区大小
    RECT clientRect;
    GetClientRect(s_targetWindow, &clientRect);

    // 计算参数区域
    RECT paramArea = CalculateParameterArea(clientRect);

    // 检查点击是否在参数区域内
    return PtInRect(&paramArea, clientPt);
}