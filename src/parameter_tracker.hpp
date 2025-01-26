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
#include <array>

class ParameterTracker {
public:
    explicit ParameterTracker(HWND targetWindow);
    ~ParameterTracker();
    
    bool Initialize();

private:
    // 参数类型枚举
    enum class ParameterType {
        Vignette = 0,
        SoftLightIntensity = 1,
        SoftLightRange = 2,
        Brightness = 3,
        Exposure = 4,
        Contrast = 5,
        Saturation = 6,
        NaturalSaturation = 7,
        Highlights = 8,
        Shadows = 9
    };

    // 参数值结构体
    struct ParameterValue {
        float value = 0.0f;
        float confidence = 0.0f;
        bool is_valid = false;
    };

    // 参数存储结构
    struct Parameters {
        std::array<ParameterValue, 10> values;  // 固定大小为10的数组

        // 便捷访问方法
        ParameterValue& operator[](ParameterType type) {
            return values[static_cast<size_t>(type)];
        }
        
        const ParameterValue& operator[](ParameterType type) const {
            return values[static_cast<size_t>(type)];
        }
    };

    // 当前参数状态
    Parameters m_currentParams;

    // 更新参数值
    void UpdateParameter(ParameterType type, float raw_value, float confidence);

    // 判断参数是否为百分比类型
    bool IsPercentageParameter(ParameterType type) const;

    // 获取参数类型名称（用于调试输出）
    std::string GetParameterTypeName(ParameterType type) const;

    // OCR相关
    std::unique_ptr<ParameterOCR> m_ocr;
    std::wstring m_modelPath;

    // 内部类：捕获序列管理器
    class CaptureSequence {
    public:
        CaptureSequence(const RECT& region, ParameterTracker* tracker);
        ~CaptureSequence();
        void Start();
        bool IsActive() const { return m_isActive; }
        const RECT& GetRegion() const { return m_region; }
        int GetCaptureCount() const { return m_captureCount; }
        void SetActive(bool active) { m_isActive = active; }
        void IncrementCaptureCount() { ++m_captureCount; }
        void ProcessCapture(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, std::chrono::high_resolution_clock::time_point startTime);

        static constexpr int MAX_CAPTURES = 2;
        static constexpr int CAPTURE_INTERVAL_MS = 1;
        
    private:
        RECT m_region;
        std::atomic<bool> m_isActive{false};
        int m_captureCount = 0;
        ParameterTracker* m_tracker;
    };

    // 成员变量
    HHOOK m_mouseHook{nullptr};
    HWND m_targetWindow;
    std::shared_ptr<CaptureSequence> m_currentSequence;
    std::mutex m_sequenceMutex;
    
    // 工作线程相关
    ThreadRAII m_workerThread;
    HWND m_workerWindow{nullptr};
    std::atomic<bool> m_threadRunning{false};
    static constexpr wchar_t* WORKER_WINDOW_CLASS = L"CaptureWorkerWindow";
    
    // 钩子线程相关
    ThreadRAII m_hookThread;
    HWND m_hookWindow{nullptr};
    static constexpr wchar_t* HOOK_WINDOW_CLASS = L"CaptureHookWindow";
    
    // 成员函数
    void WorkerThreadProc();
    static LRESULT CALLBACK WorkerWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    
    void HookThreadProc();
    static LRESULT CALLBACK HookWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    
    static LRESULT CALLBACK MouseProc(int code, WPARAM wParam, LPARAM lParam);
    
    bool IsInParameterArea(const POINT& pt);
    RECT CalculateParameterArea(const RECT& clientRect);
    void StartCaptureSequence(const RECT& region);

    // 参数区域的比例常量
    static constexpr double AREA_WIDTH_RATIO = 0.425;   // 非宽幅时，区域宽度占窗口宽度的固定比例约为42.5%
    static constexpr double AREA_HEIGHT_RATIO = 0.475;  // 宽幅时，区域高度占窗口高度的固定比例约为47.5%
    static constexpr double AREA_ASPECT_RATIO = 1.585;  // 区域的宽高比固定约为1.585:1

    // 值区域结构
    struct ValueRegion {
        int y;
        RECT bounds;
        ParameterType type;  // 添加参数类型字段
    };

    // 图像处理函数
    HRESULT ConvertToGrayscale(IWICBitmapSource* source, Microsoft::WRL::ComPtr<IWICBitmapSource>& result);
    HRESULT ResizeTo512LongEdge(IWICBitmapSource* source, Microsoft::WRL::ComPtr<IWICBitmapSource>& result);
    HRESULT Binarize(IWICBitmapSource* source, BYTE threshold, Microsoft::WRL::ComPtr<IWICBitmapSource>& result);
    HRESULT Trinarize(IWICBitmapSource* source, BYTE lowerThreshold, BYTE upperThreshold, Microsoft::WRL::ComPtr<IWICBitmapSource>& result);
    HRESULT CropScrollbarRegion(IWICBitmapSource* source, Microsoft::WRL::ComPtr<IWICBitmapSource>& result);
    bool DetectScrollbarPosition(IWICBitmapSource* binary, int& centerY);
    bool GetValueRegions(IWICBitmapSource* binary, int scrollbarCenterY, std::vector<ValueRegion>& regions);
    HRESULT CropValueRegion(IWICBitmapSource* source, const ValueRegion& region, Microsoft::WRL::ComPtr<IWICBitmapSource>& result);
    HRESULT CropNumberRegion(IWICBitmapSource* source, Microsoft::WRL::ComPtr<IWICBitmapSource>& result);

    // 静态实例指针，用于回调函数
    static ParameterTracker* s_instance;
}; 