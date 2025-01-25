#include "parameter_tracker.hpp"
#include "window_utils.hpp"
#include "parameter_ocr.hpp"
#include "image_processor.hpp"
#include <fstream>

// 静态成员定义
ParameterTracker* ParameterTracker::s_instance = nullptr;

ParameterTracker::ParameterTracker(HWND targetWindow)
    : m_targetWindow(targetWindow)
    , m_modelPath(L"models/model.onnx")
{
    s_instance = this;
}

ParameterTracker::~ParameterTracker() {
    m_threadRunning = false;

    // 清理当前捕获序列
    {
        std::lock_guard<std::mutex> lock(m_sequenceMutex);
        if (m_currentSequence) {
            m_currentSequence->SetActive(false);
            m_currentSequence.reset();
        }
    }

    // 清理鼠标钩子
    if (m_mouseHook) {
        UnhookWindowsHookEx(m_mouseHook);
        m_mouseHook = nullptr;
    }

    // 向线程发送退出消息
    if (m_workerThread.get()) {
        PostThreadMessage(m_workerThread.get_id(), WM_QUIT, 0, 0);
    }
    if (m_hookThread.get()) {
        PostThreadMessage(m_hookThread.get_id(), WM_QUIT, 0, 0);
    }

    s_instance = nullptr;
}

bool ParameterTracker::Initialize() {
    // 检查模型文件是否存在
    DWORD attrs = GetFileAttributesW(m_modelPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        OutputDebugStringA("OCR模型文件不存在: ");
        OutputDebugStringW(m_modelPath.c_str());
        OutputDebugStringA("\n");
        return false;
    }

    // 初始化OCR
    try {
        m_ocr = std::make_unique<ParameterOCR>(m_modelPath);
    } catch (const std::exception& e) {
        OutputDebugStringA("OCR初始化失败: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        return false;
    }

    // 启动工作线程
    m_threadRunning = true;
    try {
        m_workerThread = ThreadRAII([this]() { this->WorkerThreadProc(); });
        m_hookThread = ThreadRAII([this]() { this->HookThreadProc(); });
    } catch (const std::exception& e) {
        OutputDebugStringA("线程启动失败: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        m_threadRunning = false;
        return false;
    }

    return true;
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
    m_hookWindow = CreateWindowEx(
        0, HOOK_WINDOW_CLASS, L"HookWindow",
        WS_OVERLAPPED,
        0, 0, 0, 0,
        HWND_MESSAGE, nullptr, GetModuleHandle(NULL), nullptr
    );

    if (!m_hookWindow) {
        CoUninitialize();
        return;
    }

    // 安装鼠标钩子
    m_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, 
        GetModuleHandle(NULL), 0);
        
    if (!m_mouseHook) {
        DestroyWindow(m_hookWindow);
        m_hookWindow = NULL;
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
    if (m_mouseHook) {
        UnhookWindowsHookEx(m_mouseHook);
        m_mouseHook = NULL;
    }

    if (m_hookWindow) {
        DestroyWindow(m_hookWindow);
        m_hookWindow = NULL;
        UnregisterClass(HOOK_WINDOW_CLASS, GetModuleHandle(NULL));
    }
    
    // COM 反初始化（确保在线程结束前调用）
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
        m_threadRunning = false;
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
    m_workerWindow = CreateWindowEx(
        0, WORKER_WINDOW_CLASS, L"CaptureWorker",
        WS_OVERLAPPED,
        0, 0, 0, 0,
        HWND_MESSAGE, nullptr, GetModuleHandle(NULL), nullptr
    );

    if (!m_workerWindow) {
        CoUninitialize();
        m_threadRunning = false;
        return;
    }

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理窗口
    if (m_workerWindow) {
        DestroyWindow(m_workerWindow);
        m_workerWindow = NULL;
        UnregisterClass(WORKER_WINDOW_CLASS, GetModuleHandle(NULL));
    }

    // COM 反初始化（确保在线程结束前调用）
    CoUninitialize();
    m_threadRunning = false;
}

// 工作线程消息处理
LRESULT CALLBACK ParameterTracker::WorkerWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (!s_instance) return DefWindowProc(hwnd, msg, wp, lp);
    
    switch (msg) {
        case Constants::WM_USER_START_CAPTURE: {
            RECT clientRect;
            GetClientRect(s_instance->m_targetWindow, &clientRect);
            RECT paramArea = s_instance->CalculateParameterArea(clientRect);
            s_instance->StartCaptureSequence(paramArea);
            return 0;
        }

        case Constants::WM_PARAMETER_START_CAPTURE: {
            std::lock_guard<std::mutex> lock(s_instance->m_sequenceMutex);
            if (!s_instance->m_currentSequence || !s_instance->m_currentSequence->IsActive()) {
                return 0;
            }

            RECT cropRegion = s_instance->m_currentSequence->GetRegion();
            
            if (s_instance->m_currentSequence->GetCaptureCount() == 0) {
                if (!WindowUtils::BeginCaptureSession(s_instance->m_targetWindow, &cropRegion)) {
                    return 0;
                }
            }

            auto startTime = std::chrono::high_resolution_clock::now();
            
            WindowUtils::RequestNextFrame([startTime, sequence = s_instance->m_currentSequence](Microsoft::WRL::ComPtr<ID3D11Texture2D> texture) {
                sequence->ProcessCapture(texture, startTime);

                if (sequence->IsActive() && sequence->GetCaptureCount() < CaptureSequence::MAX_CAPTURES) {
                    SetTimer(s_instance->m_workerWindow, 1, CaptureSequence::CAPTURE_INTERVAL_MS, nullptr);
                } else {
                    sequence->SetActive(false);
                    WindowUtils::EndCaptureSession();
                }
            });
            
            return 0;
        }

        case WM_TIMER: {
            KillTimer(hwnd, wp);
            std::lock_guard<std::mutex> lock(s_instance->m_sequenceMutex);
            if (s_instance->m_currentSequence && s_instance->m_currentSequence->IsActive()) {
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
    if (code >= 0 && wParam == WM_LBUTTONDOWN && s_instance) {
        MSLLHOOKSTRUCT* hookStruct = (MSLLHOOKSTRUCT*)lParam;
        POINT pt = hookStruct->pt;
        
        if (s_instance->m_targetWindow && IsWindow(s_instance->m_targetWindow) && 
            s_instance->IsInParameterArea(pt)) {
            PostMessage(s_instance->m_workerWindow, Constants::WM_USER_START_CAPTURE, 0, 0);
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

// 开始新的捕获序列
void ParameterTracker::StartCaptureSequence(const RECT& region) {
    std::lock_guard<std::mutex> lock(m_sequenceMutex);
    
    // 如果当前有活动的序列且未完成，则忽略新的请求
    if (m_currentSequence && m_currentSequence->IsActive()) {
        return;
    }

    // 创建并启动新的捕获序列
    m_currentSequence = std::make_shared<CaptureSequence>(region, this);
    m_currentSequence->Start();
}

// 捕获序列启动
void ParameterTracker::CaptureSequence::Start() {
    if (m_isActive) return;
    
    m_isActive = true;
    m_captureCount = 0;
    
    // 发送开始捕获消息到工作线程
    if (m_tracker->m_workerWindow) {
        PostMessage(m_tracker->m_workerWindow, Constants::WM_PARAMETER_START_CAPTURE, 0, 0);
    }
}

// 捕获序列实现
ParameterTracker::CaptureSequence::CaptureSequence(const RECT& region, ParameterTracker* tracker)
    : m_region(region)
    , m_tracker(tracker)
{
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
    if (FAILED(m_tracker->ConvertToGrayscale(originalBitmap.Get(), grayBitmap))) {
        return;
    }
    
    Microsoft::WRL::ComPtr<IWICBitmapSource> resizedBitmap;
    if (FAILED(m_tracker->ResizeTo512LongEdge(grayBitmap.Get(), resizedBitmap))) {
        return;
    }

    // 3. 二值化用于滚动条检测
    Microsoft::WRL::ComPtr<IWICBitmapSource> scrollbarBinary;
    if (FAILED(m_tracker->Binarize(resizedBitmap.Get(), 210, scrollbarBinary))) {
        return;
    }

    // 3.1 裁剪滚动条区域
    Microsoft::WRL::ComPtr<IWICBitmapSource> scrollbarCropped;
    if (FAILED(m_tracker->CropScrollbarRegion(scrollbarBinary.Get(), scrollbarCropped))) {
        return;
    }

    // 4. 检测滚动条位置
    int scrollbarCenterY;
    if (!m_tracker->DetectScrollbarPosition(scrollbarCropped.Get(), scrollbarCenterY)) {
        return;
    }

    // 5. 二值化用于菜单检测
    Microsoft::WRL::ComPtr<IWICBitmapSource> menuBinary;
    if (FAILED(m_tracker->Binarize(resizedBitmap.Get(), 85, menuBinary))) {
        return;
    }

    // 6. 获取值区域位置
    std::vector<ValueRegion> valueRegions;
    if (!m_tracker->GetValueRegions(menuBinary.Get(), scrollbarCenterY, valueRegions)) {
        return;
    }

    // 7. 三值化处理
    Microsoft::WRL::ComPtr<IWICBitmapSource> trinaryBitmap;
    if (FAILED(m_tracker->Trinarize(resizedBitmap.Get(), 90, 110, trinaryBitmap))) {
        return;
    }

    // 8. 裁剪并保存值区域
    auto now = std::chrono::system_clock::now();
    std::wstring baseName = L"Parameter_" + 
        std::to_wstring(std::chrono::system_clock::to_time_t(now)) + 
        L"_" + std::to_wstring(m_captureCount);
    std::wstring outputDir = WindowUtils::GetScreenshotPath();

    // OCR处理
    try {
        for (size_t i = 0; i < valueRegions.size(); ++i) {
            const auto& region = valueRegions[i];

            // 裁剪值区域
            Microsoft::WRL::ComPtr<IWICBitmapSource> croppedRegion;
            if (FAILED(m_tracker->CropValueRegion(trinaryBitmap.Get(), region, croppedRegion))) {
                continue;
            }

            // 精确裁剪
            Microsoft::WRL::ComPtr<IWICBitmapSource> finalRegion;
            if (FAILED(m_tracker->CropNumberRegion(croppedRegion.Get(), finalRegion))) {
                continue;
            }

            // 缩放到24x10
            Microsoft::WRL::ComPtr<IWICBitmapSource> resizedRegion = ImageProcessor::Resize(finalRegion.Get(), 24, 10);
            if (!resizedRegion) {
                OutputDebugStringA("缩放图像失败\n");
                continue;
            }

            // 预处理图像数据
            std::vector<float> preprocessed_data = m_tracker->m_ocr->preprocess_image(resizedRegion.Get());

            // 执行OCR预测
            auto prediction = m_tracker->m_ocr->predict(preprocessed_data);

            // 输出调试信息
            std::string debug_msg = "OCR Result for region " + std::to_string(i) + 
                                  " (y=" + std::to_string(region.y) + "): " +
                                  prediction.value + " (type: " + prediction.type + 
                                  ", confidence: " + std::to_string(prediction.confidence) + ")\n";
            OutputDebugStringA(debug_msg.c_str());
        }
    } catch (const std::exception& e) {
        OutputDebugStringA("OCR处理错误: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
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
    if (!m_targetWindow || !IsWindow(m_targetWindow)) return false;

    // 转换屏幕坐标到窗口坐标
    POINT clientPt = pt;
    if (!ScreenToClient(m_targetWindow, &clientPt)) return false;

    // 获取窗口客户区大小
    RECT clientRect;
    GetClientRect(m_targetWindow, &clientRect);

    // 计算参数区域
    RECT paramArea = CalculateParameterArea(clientRect);

    // 检查点击是否在参数区域内
    return PtInRect(&paramArea, clientPt);
}

// 灰度转换实现
HRESULT ParameterTracker::ConvertToGrayscale(IWICBitmapSource* source, Microsoft::WRL::ComPtr<IWICBitmapSource>& result) {
    result = ImageProcessor::ConvertToGrayscale(source);
    return result ? S_OK : E_FAIL;
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

    // 使用ImageProcessor的缩放功能
    result = ImageProcessor::Resize(source, targetWidth, targetHeight);
    return result ? S_OK : E_FAIL;
}

// 二值化实现
HRESULT ParameterTracker::Binarize(IWICBitmapSource* source, BYTE threshold, Microsoft::WRL::ComPtr<IWICBitmapSource>& result) {
    result = ImageProcessor::Binarize(source, threshold);
    return result ? S_OK : E_FAIL;
}

// 三值化实现
HRESULT ParameterTracker::Trinarize(IWICBitmapSource* source, BYTE lowerThreshold, BYTE upperThreshold, Microsoft::WRL::ComPtr<IWICBitmapSource>& result) {
    result = ImageProcessor::Trinarize(source, lowerThreshold, upperThreshold);
    return result ? S_OK : E_FAIL;
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
    if (!source) return E_INVALIDARG;

    result = ImageProcessor::Crop(source, region.bounds);
    return result ? S_OK : E_FAIL;
}

// 精确裁剪数字区域实现
HRESULT ParameterTracker::CropNumberRegion(IWICBitmapSource* source, Microsoft::WRL::ComPtr<IWICBitmapSource>& result) {
    result = ImageProcessor::AutoCropNumber(source);
    return result ? S_OK : E_FAIL;
}

// 裁剪滚动条区域实现
HRESULT ParameterTracker::CropScrollbarRegion(IWICBitmapSource* source, Microsoft::WRL::ComPtr<IWICBitmapSource>& result) {
    if (!source) return E_INVALIDARG;

    // 固定的滚动条区域
    RECT rect = {
        504,  // left
        92,   // top
        511,  // right (504 + 7)
        319   // bottom (92 + 227)
    };

    result = ImageProcessor::Crop(source, rect);
    return result ? S_OK : E_FAIL;
}