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
std::unique_ptr<std::thread> ParameterTracker::s_hookThread;
HWND ParameterTracker::s_workerWindow = NULL;
HWND ParameterTracker::s_hookWindow = NULL;
std::atomic<bool> ParameterTracker::s_threadRunning{false};
ParameterTracker::AllParameters ParameterTracker::s_currentParams;

// 初始化参数追踪器
bool ParameterTracker::Initialize(HWND targetWindow) {
    s_targetWindow = targetWindow;
    
    // 启动工作线程
    s_threadRunning = true;
    s_workerThread = std::make_unique<std::thread>(&WorkerThreadProc);
    
    // 启动钩子线程
    s_hookThread = std::make_unique<std::thread>(&HookThreadProc);

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

    // 清理钩子线程
    if (s_hookWindow) {
        PostMessage(s_hookWindow, WM_QUIT, 0, 0);
    }
    if (s_hookThread && s_hookThread->joinable()) {
        s_hookThread->join();
    }
    s_hookThread.reset();

    // 清理窗口和COM
    if (s_workerWindow) {
        DestroyWindow(s_workerWindow);
        s_workerWindow = NULL;
        UnregisterClass(WORKER_WINDOW_CLASS, GetModuleHandle(NULL));
    }
    
    if (s_hookWindow) {
        DestroyWindow(s_hookWindow);
        s_hookWindow = NULL;
        UnregisterClass(HOOK_WINDOW_CLASS, GetModuleHandle(NULL));
    }

    CoUninitialize();
    s_targetWindow = NULL;
}

// 钩子线程函数
void ParameterTracker::HookThreadProc() {
    // 初始化COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return;
    }

    // 注册窗口类
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = HookWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = HOOK_WINDOW_CLASS;
    
    RegisterClassEx(&wc);
    
    // 创建钩子线程的消息窗口
    s_hookWindow = CreateWindowEx(
        0, HOOK_WINDOW_CLASS, L"HookWindow",
        WS_OVERLAPPED,
        0, 0, 0, 0,
        HWND_MESSAGE, nullptr, GetModuleHandle(NULL), nullptr
    );

    if (!s_hookWindow) {
        CoUninitialize();
        return;
    }

    // 安装鼠标钩子
    s_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, 
        GetModuleHandle(NULL), 0);
        
    if (!s_mouseHook) {
        DestroyWindow(s_hookWindow);
        s_hookWindow = NULL;
        UnregisterClass(HOOK_WINDOW_CLASS, GetModuleHandle(NULL));
        CoUninitialize();
        return;
    }

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理
    if (s_mouseHook) {
        UnhookWindowsHookEx(s_mouseHook);
        s_mouseHook = NULL;
    }
    
    CoUninitialize();
}

// 钩子窗口消息处理
LRESULT CALLBACK ParameterTracker::HookWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return DefWindowProc(hwnd, msg, wp, lp);
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

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 只设置线程状态
    s_threadRunning = false;
    CoUninitialize();
}

// 工作线程消息处理
LRESULT CALLBACK ParameterTracker::WorkerWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case Constants::WM_USER_START_CAPTURE: {
            // 获取窗口客户区大小
            RECT clientRect;
            GetClientRect(s_targetWindow, &clientRect);

            // 计算参数区域
            RECT paramArea = CalculateParameterArea(clientRect);
            
            // 开始新的捕获序列
            StartCaptureSequence(paramArea);
            return 0;
        }

        case Constants::WM_PARAMETER_START_CAPTURE: {
            std::lock_guard<std::mutex> lock(s_sequenceMutex);
            if (!s_currentSequence || !s_currentSequence->IsActive()) {
                return 0;
            }

            // 获取当前序列的裁剪区域
            RECT cropRegion = s_currentSequence->GetRegion();
            
            // 只在序列第一次捕获时初始化会话
            if (s_currentSequence->GetCaptureCount() == 0) {
                if (!WindowUtils::BeginCaptureSession(s_targetWindow, &cropRegion)) {
                    return 0;
                }
            }

            // 记录开始时间
            auto startTime = std::chrono::high_resolution_clock::now();
            
            // 请求下一帧
            WindowUtils::RequestNextFrame([startTime, sequence = s_currentSequence](Microsoft::WRL::ComPtr<ID3D11Texture2D> texture) {
                sequence->ProcessCapture(texture, startTime);

                // 如果还需要继续捕获，只设置定时器
                if (sequence->IsActive() && sequence->GetCaptureCount() < CaptureSequence::MAX_CAPTURES) {
                    SetTimer(s_workerWindow, 1, CaptureSequence::CAPTURE_INTERVAL_MS, nullptr);
                } else {
                    // 只在整个序列结束时才结束会话
                    sequence->SetActive(false);
                    WindowUtils::EndCaptureSession();
                }
            });
            
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

// 鼠标钩子回调
LRESULT CALLBACK ParameterTracker::MouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && wParam == WM_LBUTTONDOWN) {
        MSLLHOOKSTRUCT* hookStruct = (MSLLHOOKSTRUCT*)lParam;
        POINT pt = hookStruct->pt;
        
        if (s_targetWindow && IsWindow(s_targetWindow) && IsInParameterArea(pt)) {
            // 只发送消息到工作线程，立即返回
            PostMessage(s_workerWindow, Constants::WM_USER_START_CAPTURE, 0, 0);
        }
    }
    return CallNextHookEx(s_mouseHook, code, wParam, lParam);
}

// 开始新的捕获序列
void ParameterTracker::StartCaptureSequence(const RECT& region) {
    std::lock_guard<std::mutex> lock(s_sequenceMutex);
    
    // 如果当前有活动的序列且未完成，则忽略新的请求
    if (s_currentSequence && s_currentSequence->IsActive()) {
        return;
    }

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
    if (m_isActive) {
        WindowUtils::EndCaptureSession();
        m_isActive = false;
    }
}

// 处理捕获的实现
void ParameterTracker::CaptureSequence::ProcessCapture(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, std::chrono::high_resolution_clock::time_point startTime) {
    // 1. 转换为WIC位图
    Microsoft::WRL::ComPtr<IWICBitmapSource> originalBitmap;
    if (FAILED(WindowUtils::TextureToWICBitmap(texture.Get(), originalBitmap))) {
        return;
    }

    // 2. 转灰度并缩放到512长边
    Microsoft::WRL::ComPtr<IWICBitmapSource> grayBitmap;
    if (FAILED(ConvertToGrayscale(originalBitmap.Get(), grayBitmap))) {
        return;
    }
    
    Microsoft::WRL::ComPtr<IWICBitmapSource> resizedBitmap;
    if (FAILED(ResizeTo512LongEdge(grayBitmap.Get(), resizedBitmap))) {
        return;
    }

    // 3. 二值化用于滚动条检测
    Microsoft::WRL::ComPtr<IWICBitmapSource> scrollbarBinary;
    if (FAILED(Binarize(resizedBitmap.Get(), 210, scrollbarBinary))) {
        return;
    }

    // 3.1 裁剪滚动条区域
    Microsoft::WRL::ComPtr<IWICBitmapSource> scrollbarCropped;
    if (FAILED(CropScrollbarRegion(scrollbarBinary.Get(), scrollbarCropped))) {
        return;
    }

    // 4. 检测滚动条位置
    int scrollbarCenterY;
    if (!DetectScrollbarPosition(scrollbarCropped.Get(), scrollbarCenterY)) {
        return;
    }

    // 5. 二值化用于菜单检测
    Microsoft::WRL::ComPtr<IWICBitmapSource> menuBinary;
    if (FAILED(Binarize(resizedBitmap.Get(), 85, menuBinary))) {
        return;
    }

    // 6. 获取值区域位置
    std::vector<ValueRegion> valueRegions;
    if (!GetValueRegions(menuBinary.Get(), scrollbarCenterY, valueRegions)) {
        return;
    }

    // 7. 三值化处理
    Microsoft::WRL::ComPtr<IWICBitmapSource> trinaryBitmap;
    if (FAILED(Trinarize(resizedBitmap.Get(), 90, 110, trinaryBitmap))) {
        return;
    }

    // 8. 裁剪并保存值区域
    auto now = std::chrono::system_clock::now();
    std::wstring baseName = L"Parameter_" + 
        std::to_wstring(std::chrono::system_clock::to_time_t(now)) + 
        L"_" + std::to_wstring(m_captureCount);
    std::wstring outputDir = WindowUtils::GetScreenshotPath();

    for (size_t i = 0; i < valueRegions.size(); ++i) {
        const auto& region = valueRegions[i];

        // 裁剪值区域
        Microsoft::WRL::ComPtr<IWICBitmapSource> croppedRegion;
        if (FAILED(CropValueRegion(trinaryBitmap.Get(), region, croppedRegion))) {
            continue;
        }

        // 保存裁剪后的图像
        {
            std::wstring croppedFileName = baseName + L"_value_" + 
                std::to_wstring(i) + L"_" + std::to_wstring(region.y) + L".png";
            std::wstring croppedPath = outputDir + L"\\" + croppedFileName;
            WindowUtils::SaveWICBitmapToFile(croppedRegion.Get(), croppedPath);
        }

        // 精确裁剪
        Microsoft::WRL::ComPtr<IWICBitmapSource> finalRegion;
        if (FAILED(CropNumberRegion(croppedRegion.Get(), finalRegion))) {
            continue;
        }

        // 生成输出文件名
        std::wstring filename = baseName + L"_value_" + 
            std::to_wstring(i) + L"_" + std::to_wstring(region.y) + L".png";
        std::wstring outputPath = outputDir + L"\\" + filename;

        // 保存图片
        WindowUtils::SaveWICBitmapToFile(finalRegion.Get(), outputPath);
    }

    // 增加捕获计数
    IncrementCaptureCount();
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

// 灰度转换实现
HRESULT ParameterTracker::ConvertToGrayscale(IWICBitmapSource* source, Microsoft::WRL::ComPtr<IWICBitmapSource>& result) {
    if (!source) return E_INVALIDARG;

    // 创建WIC工厂
    Microsoft::WRL::ComPtr<IWICImagingFactory2> factory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory2,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)
    );
    if (FAILED(hr)) return hr;

    // 创建颜色转换器
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return hr;

    // 初始化转换器为灰度格式
    hr = converter->Initialize(
        source,                          // 输入源
        GUID_WICPixelFormat8bppGray,    // 目标格式：8位灰度
        WICBitmapDitherTypeNone,        // 不使用抖动
        nullptr,                         // 不使用调色板
        0.0f,                           // 透明度阈值
        WICBitmapPaletteTypeCustom      // 自定义调色板
    );
    if (FAILED(hr)) return hr;

    // 返回结果
    return converter.As(&result);
}

// 缩放到512长边实现
HRESULT ParameterTracker::ResizeTo512LongEdge(IWICBitmapSource* source, Microsoft::WRL::ComPtr<IWICBitmapSource>& result) {
    if (!source) return E_INVALIDARG;

    // 获取原始尺寸
    UINT originalWidth, originalHeight;
    HRESULT hr = source->GetSize(&originalWidth, &originalHeight);
    if (FAILED(hr)) return hr;

    // 计算目标尺寸
    UINT targetWidth, targetHeight;
    if (originalWidth > originalHeight) {
        targetWidth = 512;
        targetHeight = static_cast<UINT>((512.0 * originalHeight) / originalWidth);
    } else {
        targetHeight = 512;
        targetWidth = static_cast<UINT>((512.0 * originalWidth) / originalHeight);
    }

    // 使用WindowUtils的缩放功能
    result = WindowUtils::ResizeWICBitmap(source, targetWidth, targetHeight);
    return result ? S_OK : E_FAIL;
}

// 二值化实现
HRESULT ParameterTracker::Binarize(IWICBitmapSource* source, BYTE threshold, Microsoft::WRL::ComPtr<IWICBitmapSource>& result) {
    if (!source) return E_INVALIDARG;

    // 获取源图像尺寸
    UINT width = 0, height = 0;
    HRESULT hr = source->GetSize(&width, &height);
    if (FAILED(hr)) return hr;

    // 创建WIC工厂
    Microsoft::WRL::ComPtr<IWICImagingFactory2> factory;
    hr = CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return hr;

    // 创建新的位图
    Microsoft::WRL::ComPtr<IWICBitmap> binaryBitmap;
    hr = factory->CreateBitmap(width, height, GUID_WICPixelFormat8bppGray, WICBitmapCacheOnLoad, &binaryBitmap);
    if (FAILED(hr)) return hr;

    // 读取源数据
    UINT sourceStride = ((width * 8 + 31) / 32) * 4; // 8bpp的stride计算
    std::vector<BYTE> sourceBuffer(sourceStride * height);
    hr = source->CopyPixels(nullptr, sourceStride, sourceStride * height, sourceBuffer.data());
    if (FAILED(hr)) return hr;

    // 锁定目标位图进行写入
    WICRect lockRect = { 0, 0, static_cast<INT>(width), static_cast<INT>(height) };
    Microsoft::WRL::ComPtr<IWICBitmapLock> lock;
    hr = binaryBitmap->Lock(&lockRect, WICBitmapLockWrite, &lock);
    if (FAILED(hr)) return hr;

    // 获取目标数据指针和步长
    UINT targetStride = 0;
    UINT bufferSize = 0;
    BYTE* targetData = nullptr;
    hr = lock->GetStride(&targetStride);
    if (FAILED(hr)) return hr;
    hr = lock->GetDataPointer(&bufferSize, &targetData);
    if (FAILED(hr)) return hr;

    // 执行二值化
    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            BYTE sourceValue = sourceBuffer[y * sourceStride + x];
            targetData[y * targetStride + x] = (sourceValue > threshold) ? 255 : 0;
        }
    }

    // 返回结果
    return binaryBitmap.As(&result);
}

// 三值化实现
HRESULT ParameterTracker::Trinarize(IWICBitmapSource* source, BYTE lowerThreshold, BYTE upperThreshold, Microsoft::WRL::ComPtr<IWICBitmapSource>& result) {
    if (!source) return E_INVALIDARG;

    // 获取源图像尺寸
    UINT width = 0, height = 0;
    HRESULT hr = source->GetSize(&width, &height);
    if (FAILED(hr)) return hr;

    // 创建WIC工厂
    Microsoft::WRL::ComPtr<IWICImagingFactory2> factory;
    hr = CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return hr;

    // 创建新的位图
    Microsoft::WRL::ComPtr<IWICBitmap> trinaryBitmap;
    hr = factory->CreateBitmap(width, height, GUID_WICPixelFormat8bppGray, WICBitmapCacheOnLoad, &trinaryBitmap);
    if (FAILED(hr)) return hr;

    // 读取源数据
    UINT sourceStride = ((width * 8 + 31) / 32) * 4; // 8bpp的stride计算
    std::vector<BYTE> sourceBuffer(sourceStride * height);
    hr = source->CopyPixels(nullptr, sourceStride, sourceStride * height, sourceBuffer.data());
    if (FAILED(hr)) return hr;

    // 锁定目标位图进行写入
    WICRect lockRect = { 0, 0, static_cast<INT>(width), static_cast<INT>(height) };
    Microsoft::WRL::ComPtr<IWICBitmapLock> lock;
    hr = trinaryBitmap->Lock(&lockRect, WICBitmapLockWrite, &lock);
    if (FAILED(hr)) return hr;

    // 获取目标数据指针和步长
    UINT targetStride = 0;
    UINT bufferSize = 0;
    BYTE* targetData = nullptr;
    hr = lock->GetStride(&targetStride);
    if (FAILED(hr)) return hr;
    hr = lock->GetDataPointer(&bufferSize, &targetData);
    if (FAILED(hr)) return hr;

    // 执行三值化
    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            BYTE sourceValue = sourceBuffer[y * sourceStride + x];
            if (sourceValue > upperThreshold) {
                targetData[y * targetStride + x] = 255;  // 白色
            } else if (sourceValue > lowerThreshold) {
                targetData[y * targetStride + x] = 128;  // 灰色
            } else {
                targetData[y * targetStride + x] = 0;    // 黑色
            }
        }
    }

    // 返回结果
    return trinaryBitmap.As(&result);
}

// 滚动条位置检测实现
bool ParameterTracker::DetectScrollbarPosition(IWICBitmapSource* binary, int& centerY) {
    if (!binary) return false;

    // 获取图像尺寸
    UINT width = 0, height = 0;
    HRESULT hr = binary->GetSize(&width, &height);
    if (FAILED(hr)) return false;

    // 分配内存用于存储图像数据
    std::vector<BYTE> buffer(width * height);
    hr = binary->CopyPixels(nullptr, width, width * height, buffer.data());
    if (FAILED(hr)) return false;

    // 收集所有白色像素的y坐标
    std::vector<int> whitePixelYCoords;
    whitePixelYCoords.reserve(height);  // 预分配空间以提高性能
    
    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            if (buffer[y * width + x] == 255) {  // 白色像素
                whitePixelYCoords.push_back(y);
                break;  // 找到该行的白色像素后就可以继续下一行
            }
        }
    }
    
    if (whitePixelYCoords.empty()) {
        return false;
    }

    // 对y坐标进行排序并去重
    std::sort(whitePixelYCoords.begin(), whitePixelYCoords.end());
    auto last = std::unique(whitePixelYCoords.begin(), whitePixelYCoords.end());
    whitePixelYCoords.erase(last, whitePixelYCoords.end());

    // 如果白色像素太少，认为检测失败
    if (whitePixelYCoords.size() < 8) {  // 需要至少8个点（顶部4个和底部4个）
        return false;
    }

    // 获取顶部和底部候选点
    std::vector<int> topCandidates(whitePixelYCoords.begin(), whitePixelYCoords.begin() + 4);
    std::vector<int> bottomCandidates(whitePixelYCoords.end() - 4, whitePixelYCoords.end());

    const int threshold = 5;  // y轴差值阈值
    
    // 找到合适的顶部位置
    int topY = topCandidates[0];
    for (size_t i = 0; i < topCandidates.size() - 1; ++i) {
        if (std::abs(topCandidates[i] - topCandidates[i + 1]) <= threshold) {
            topY = topCandidates[i];
            break;
        }
    }

    // 找到合适的底部位置
    int bottomY = bottomCandidates.back();
    for (size_t i = bottomCandidates.size() - 1; i > 0; --i) {
        if (std::abs(bottomCandidates[i] - bottomCandidates[i - 1]) <= threshold) {
            bottomY = bottomCandidates[i];
            break;
        }
    }

    // 计算中心位置
    centerY = (topY + bottomY) / 2;
    
    return true;
}

// 获取值区域实现
bool ParameterTracker::GetValueRegions(IWICBitmapSource* binary, int scrollbarCenterY, std::vector<ValueRegion>& regions) {
    if (!binary) return false;

    // 获取图像尺寸
    UINT width = 0, height = 0;
    HRESULT hr = binary->GetSize(&width, &height);
    if (FAILED(hr)) return false;

    // 滚动条参数
    const double SCROLL_TOP_Y = 61.0;  // 滚动条最顶部中心y值
    const double SCROLL_BOTTOM_Y = 163.0;  // 滚动条最底部中心y值
    const double SCROLL_RANGE = SCROLL_BOTTOM_Y - SCROLL_TOP_Y;  // 滚动条可移动范围

    // 菜单参数（基于原始图片尺寸）
    const double MENU_ITEM_HEIGHT = 46.4;  // 每个菜单项的高度
    const double TOTAL_MENU_HEIGHT = 464.0;  // 总菜单高度（10个菜单项）
    const double VISIBLE_HEIGHT = 231.0;  // 可见区域高度（5个菜单项）
    const double FIRST_MENU_Y = 109.5;  // 第一个菜单项的中心y坐标（未滚动时）
    const double TOLERANCE = 3.0;  // 容差值（像素）

    // 值区域的固定参数
    const int VALUE_X_START = 441;
    const int VALUE_X_END = 473;
    const int VALUE_HEIGHT = 18;

    // 计算滚动偏移量
    double scrollRatio = (scrollbarCenterY - SCROLL_TOP_Y) / SCROLL_RANGE;
    double scrollOffset = scrollRatio * (TOTAL_MENU_HEIGHT - VISIBLE_HEIGHT);

    // 计算所有可能可见的菜单项位置
    std::vector<int> menuPositions;
    for (int i = 0; i < 12; ++i) {  // 多计算几个确保覆盖边界情况
        double menuY = FIRST_MENU_Y + i * MENU_ITEM_HEIGHT - scrollOffset;
        // 检查y坐标是否在有效范围内
        if (menuY >= (FIRST_MENU_Y - TOLERANCE) && 
            menuY <= (height - MENU_ITEM_HEIGHT - TOLERANCE)) {
            menuPositions.push_back(static_cast<int>(round(menuY)));
        }
    }

    // 创建值区域
    regions.clear();
    for (int y : menuPositions) {
        ValueRegion region;
        region.y = y;
        region.bounds.left = VALUE_X_START;
        region.bounds.right = VALUE_X_END;
        region.bounds.top = y - VALUE_HEIGHT/2;
        region.bounds.bottom = y + VALUE_HEIGHT/2;

        // 确保裁剪区域不超出图片边界
        if (region.bounds.top >= 0 && region.bounds.bottom < static_cast<int>(height)) {
            regions.push_back(region);
        }
    }

    return !regions.empty();
}

// 裁剪值区域实现
HRESULT ParameterTracker::CropValueRegion(IWICBitmapSource* source, const ValueRegion& region, Microsoft::WRL::ComPtr<IWICBitmapSource>& result) {
    if (!source) {
        return E_INVALIDARG;
    }

    // 获取源图像尺寸
    UINT width = 0, height = 0;
    HRESULT hr = source->GetSize(&width, &height);
    if (FAILED(hr)) {
        return hr;
    }

    // 验证裁剪区域是否有效
    if (region.bounds.left < 0 || region.bounds.top < 0 ||
        region.bounds.right > static_cast<int>(width) ||
        region.bounds.bottom > static_cast<int>(height) ||
        region.bounds.left >= region.bounds.right ||
        region.bounds.top >= region.bounds.bottom) {
        return E_INVALIDARG;
    }

    // 创建WIC工厂
    Microsoft::WRL::ComPtr<IWICImagingFactory2> factory;
    hr = CoCreateInstance(
        CLSID_WICImagingFactory2,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)
    );
    if (FAILED(hr)) {
        return hr;
    }

    // 创建裁剪器
    Microsoft::WRL::ComPtr<IWICBitmapClipper> clipper;
    hr = factory->CreateBitmapClipper(&clipper);
    if (FAILED(hr)) {
        return hr;
    }

    // 设置裁剪区域
    WICRect rect = {
        region.bounds.left,
        region.bounds.top,
        region.bounds.right - region.bounds.left,
        region.bounds.bottom - region.bounds.top
    };

    // 初始化裁剪器
    hr = clipper->Initialize(source, &rect);
    if (FAILED(hr)) {
        return hr;
    }

    // 返回结果
    hr = clipper.As(&result);
    if (FAILED(hr)) {
        return hr;
    }

    return S_OK;
}

// 精确裁剪数字区域实现
HRESULT ParameterTracker::CropNumberRegion(IWICBitmapSource* source, Microsoft::WRL::ComPtr<IWICBitmapSource>& result) {
    if (!source) {
        return E_INVALIDARG;
    }

    // 获取源图像尺寸
    UINT width = 0, height = 0;
    HRESULT hr = source->GetSize(&width, &height);
    if (FAILED(hr)) {
        return hr;
    }

    // 读取图像数据
    std::vector<BYTE> buffer(width * height);
    hr = source->CopyPixels(nullptr, width, width * height, buffer.data());
    if (FAILED(hr)) {
        return hr;
    }

    // 计算水平投影
    std::vector<int> hProjection(height, 0);
    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            if (buffer[y * width + x] == 255) {  // 白色像素
                hProjection[y]++;
            }
        }
    }

    // 初始化边界
    int top = 0, bottom = height;
    int fullWhite = width * 255;  // 全白行的投影值
    
    // 计算中心点
    int center = (top + bottom) / 2;
    
    // 从中心向两边寻找全黑或全白行
    int newTop = top, newBottom = bottom;
    
    // 向上寻找全黑或全白行
    for (int i = center; i >= top; --i) {
        if (hProjection[i] == 0 || hProjection[i] == fullWhite) {
            newTop = i + 1;
            break;
        }
    }
    
    // 向下寻找全黑或全白行
    for (int i = center; i < bottom; ++i) {
        if (hProjection[i] == 0 || hProjection[i] == fullWhite) {
            newBottom = i;
            break;
        }
    }
    
    // 如果裁剪后高度仍然大于12px，寻找局部最小值进行进一步裁剪
    if (newBottom - newTop > 12) {
        // 收集所有非全白行的投影值和索引
        std::vector<std::pair<int, int>> candidates;  // (投影值, 索引)
        for (int i = newTop; i < newBottom; ++i) {
            if (hProjection[i] != fullWhite) {
                candidates.push_back({hProjection[i], i});
            }
        }
        
        // 按投影值从小到大排序
        std::sort(candidates.begin(), candidates.end());
        
        // 尝试每个候选点作为分割点
        for (const auto& candidate : candidates) {
            int projVal = candidate.first;
            int idx = candidate.second;
            
            // 如果在中心点上方
            if (idx < center) {
                int potentialTop = idx + 1;
                if (newBottom - potentialTop >= 9) {  // 确保裁剪后高度不小于9px
                    newTop = potentialTop;
                    break;
                }
            }
            // 如果在中心点下方
            else {
                int potentialBottom = idx;
                if (potentialBottom - newTop >= 9) {  // 确保裁剪后高度不小于9px
                    newBottom = potentialBottom;
                    break;
                }
            }
        }
    }
    
    // 在确定的上下边界区域内计算垂直投影
    std::vector<int> vProjection(width, 0);
    for (UINT x = 0; x < width; ++x) {
        for (int y = newTop; y < newBottom; ++y) {
            if (buffer[y * width + x] == 255) {  // 白色像素
                vProjection[x]++;
            }
        }
    }
    
    // 计算水平中心点和偏移区域
    int centerX = width / 2;
    int offset = 3;  // 中心偏移像素数
    int leftCenter = centerX - offset;
    int rightCenter = centerX + offset;
    
    // 第一步：从左向右找起始边界（完全黑色的列）
    int left = 0;
    while (left < width / 2 && vProjection[left] == 0) {
        left++;
    }
    
    // 第二步：从右向左找结束边界（完全黑色的列）
    int right = width - 1;
    while (right >= width / 2 && vProjection[right] == 0) {
        right--;
    }
    
    // 第三步：从中心向左寻找连续黑色区域
    int leftFromCenter = leftCenter;
    while (leftFromCenter > left) {
        // 检查连续黑色列数
        int blackCount = 0;
        int j = leftFromCenter;
        while (j > left && vProjection[j] == 0) {
            blackCount++;
            j--;
        }
        
        if (blackCount >= 5) {  // 如果有5列及以上的黑色
            left = j + 1;  // 设置为连续黑色区域的起始位置
            break;
        }
        leftFromCenter--;
    }
    
    // 第四步：从中心向右寻找连续黑色区域
    int rightFromCenter = rightCenter;
    while (rightFromCenter < right) {
        // 检查连续黑色列数
        int blackCount = 0;
        int j = rightFromCenter;
        while (j < right && vProjection[j] == 0) {
            blackCount++;
            j++;
        }
        
        if (blackCount >= 5) {  // 如果有5列及以上的黑色
            right = j - blackCount;  // 设置为连续黑色区域的前一个位置
            break;
        }
        rightFromCenter++;
    }

    // 验证边界有效性
    if (left >= right || newTop >= newBottom || 
        left < 0 || right >= static_cast<int>(width) ||
        newTop < 0 || newBottom > static_cast<int>(height)) {
        return E_FAIL;
    }

    // 创建WIC工厂
    Microsoft::WRL::ComPtr<IWICImagingFactory2> factory;
    hr = CoCreateInstance(
        CLSID_WICImagingFactory2,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)
    );
    if (FAILED(hr)) return hr;

    // 创建裁剪器
    Microsoft::WRL::ComPtr<IWICBitmapClipper> clipper;
    hr = factory->CreateBitmapClipper(&clipper);
    if (FAILED(hr)) return hr;

    // 设置裁剪区域
    WICRect rect = {
        left,
        newTop,
        right - left + 1,
        newBottom - newTop
    };

    // 初始化裁剪器
    hr = clipper->Initialize(source, &rect);
    if (FAILED(hr)) return hr;

    // 返回结果
    return clipper.As(&result);
}

// 裁剪滚动条区域实现
HRESULT ParameterTracker::CropScrollbarRegion(IWICBitmapSource* source, Microsoft::WRL::ComPtr<IWICBitmapSource>& result) {
    if (!source) return E_INVALIDARG;

    // 获取源图像尺寸
    UINT width = 0, height = 0;
    HRESULT hr = source->GetSize(&width, &height);
    if (FAILED(hr)) return hr;

    // 创建WIC工厂
    Microsoft::WRL::ComPtr<IWICImagingFactory2> factory;
    hr = CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return hr;

    // 创建裁剪器
    Microsoft::WRL::ComPtr<IWICBitmapClipper> clipper;
    hr = factory->CreateBitmapClipper(&clipper);
    if (FAILED(hr)) return hr;

    WICRect rect = {
        504,              // left
        92,                        // top
        7,          // width
        227  // height
    };

    // 初始化裁剪器
    hr = clipper->Initialize(source, &rect);
    if (FAILED(hr)) return hr;

    // 返回结果
    return clipper.As(&result);
}