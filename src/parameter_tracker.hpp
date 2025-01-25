#pragma once
#include "win_config.hpp"
#include "constants.hpp"
#include "parameter_ocr.hpp"
#include "thread_raii.hpp"
#include <functional>
#include <wrl/client.h>
#include <wincodec.h>
#include <d3d11.h>
#include <memory>
#include <chrono>
#include <mutex>
#include <vector>
#include <atomic>
#include <future>
#include <string>
#include "window_utils.hpp"

class ParameterTracker {
public:
    static bool Initialize(HWND targetWindow);
    static void Cleanup();

private:
    // 参数存储结构
    struct SceneParameters {
        float vignette = 0.0f;        // 晕影调节 (%)
        float softLightIntensity = 0.0f;  // 柔光强度 (%)
        float softLightRange = 0.0f;   // 柔光范围
        float brightness = 0.0f;       // 亮度 (%)
        float exposure = 0.0f;         // 曝光
        float saturation = 0.0f;       // 饱和度
        float naturalSaturation = 0.0f; // 自然饱和度
        float highlight = 0.0f;        // 高光
        float shadow = 0.0f;           // 阴影
    };

    struct AllParameters {
        SceneParameters scene;
    };

    // 当前参数状态（工作线程独占访问，不需要互斥锁）
    static AllParameters s_currentParams;

    // OCR相关
    static std::unique_ptr<ParameterOCR> s_ocr;
    static std::wstring s_modelPath;

    // 内部类：捕获序列管理器
    class CaptureSequence {
    public:
        CaptureSequence(const RECT& region);
        ~CaptureSequence();
        void Start();
        bool IsActive() const { return m_isActive; }
        const RECT& GetRegion() const { return m_region; }
        int GetCaptureCount() const { return m_captureCount; }
        void SetActive(bool active) { m_isActive = active; }

        // 新增：获取和增加捕获计数的方法
        void IncrementCaptureCount() { ++m_captureCount; }

        // 新增：处理捕获的方法
        void ProcessCapture(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, std::chrono::high_resolution_clock::time_point startTime);

        // 常量
        static constexpr int MAX_CAPTURES = 2;  // 最大连续捕获次数
        static constexpr int CAPTURE_INTERVAL_MS = 1;
        
    private:
        RECT m_region;
        std::atomic<bool> m_isActive{false};
        int m_captureCount = 0;
    };

    // 静态成员
    static HHOOK s_mouseHook;
    static HWND s_targetWindow;
    static std::shared_ptr<CaptureSequence> s_currentSequence;
    static std::mutex s_sequenceMutex;
    
    // 工作线程相关
    static ThreadRAII s_workerThread;
    static HWND s_workerWindow;
    static std::atomic<bool> s_threadRunning;
    static constexpr wchar_t* WORKER_WINDOW_CLASS = L"CaptureWorkerWindow";
    
    // 钩子线程相关
    static ThreadRAII s_hookThread;
    static HWND s_hookWindow;
    static constexpr wchar_t* HOOK_WINDOW_CLASS = L"CaptureHookWindow";
    
    // 工作线程函数
    static void WorkerThreadProc();
    static LRESULT CALLBACK WorkerWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    
    // 钩子线程函数
    static void HookThreadProc();
    static LRESULT CALLBACK HookWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    
    // 鼠标钩子回调
    static LRESULT CALLBACK MouseProc(int code, WPARAM wParam, LPARAM lParam);
    
    // 判断点击是否在参数区域
    static bool IsInParameterArea(const POINT& pt);

    // 计算参数区域
    static RECT CalculateParameterArea(const RECT& clientRect);

    // 开始新的捕获序列
    static void StartCaptureSequence(const RECT& region);

    // 参数区域的比例常量
    static constexpr double AREA_WIDTH_RATIO = 0.425;   // 非宽幅时，区域宽度占窗口宽度的固定比例约为42.5%
    static constexpr double AREA_HEIGHT_RATIO = 0.475;  // 宽幅时，区域高度占窗口高度的固定比例约为47.5%
    static constexpr double AREA_ASPECT_RATIO = 1.585;  // 区域的宽高比固定约为1.585:1

    // Helper structures for value region tracking
    struct ValueRegion {
        int y;          // Center y coordinate
        RECT bounds;    // Crop bounds
    };

    // Image processing helper functions
    static HRESULT ConvertToGrayscale(IWICBitmapSource* source, Microsoft::WRL::ComPtr<IWICBitmapSource>& result);
    static HRESULT ResizeTo512LongEdge(IWICBitmapSource* source, Microsoft::WRL::ComPtr<IWICBitmapSource>& result);
    static HRESULT Binarize(IWICBitmapSource* source, BYTE threshold, Microsoft::WRL::ComPtr<IWICBitmapSource>& result);
    static HRESULT Trinarize(IWICBitmapSource* source, BYTE lowerThreshold, BYTE upperThreshold, Microsoft::WRL::ComPtr<IWICBitmapSource>& result);
    static HRESULT CropScrollbarRegion(IWICBitmapSource* source, Microsoft::WRL::ComPtr<IWICBitmapSource>& result);
    static bool DetectScrollbarPosition(IWICBitmapSource* binary, int& centerY);
    static bool GetValueRegions(IWICBitmapSource* binary, int scrollbarCenterY, std::vector<ValueRegion>& regions);
    static HRESULT CropValueRegion(IWICBitmapSource* source, const ValueRegion& region, Microsoft::WRL::ComPtr<IWICBitmapSource>& result);
    static HRESULT CropNumberRegion(IWICBitmapSource* source, Microsoft::WRL::ComPtr<IWICBitmapSource>& result);
}; 