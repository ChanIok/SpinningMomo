#pragma once

#include "win_config.hpp"
#include <wincodec.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <string>
#include <filesystem>

class ImageProcessor {
public:
    // ==== 基础图像操作 ====
    static Microsoft::WRL::ComPtr<IWICBitmapSource> LoadFromFile(const std::filesystem::path& path);
    static Microsoft::WRL::ComPtr<IWICBitmapSource> ConvertToGrayscale(IWICBitmapSource* source);
    static Microsoft::WRL::ComPtr<IWICBitmapSource> Resize(
        IWICBitmapSource* source, 
        UINT width, 
        UINT height,
        WICBitmapInterpolationMode mode = WICBitmapInterpolationModeFant);
    static Microsoft::WRL::ComPtr<IWICBitmapSource> ResizeByLongEdge(
        IWICBitmapSource* source, 
        UINT longEdgeLength,
        WICBitmapInterpolationMode mode = WICBitmapInterpolationModeFant);
    
    // ==== 图像处理 ====
    static Microsoft::WRL::ComPtr<IWICBitmapSource> Binarize(
        IWICBitmapSource* source, 
        BYTE threshold);
    static Microsoft::WRL::ComPtr<IWICBitmapSource> Trinarize(
        IWICBitmapSource* source, 
        BYTE lowerThreshold, 
        BYTE upperThreshold);
    
    // ==== 格式转换 ====
    static Microsoft::WRL::ComPtr<IWICBitmapSource> TextureToWICBitmap(ID3D11Texture2D* texture);
    static bool SaveToFile(
        IWICBitmapSource* bitmap, 
        const std::wstring& filePath);
    static bool SaveToJpegFile(
        IWICBitmapSource* bitmap, 
        const std::wstring& filePath,
        float quality = 0.75f);
    
    // ==== 区域处理 ====
    static Microsoft::WRL::ComPtr<IWICBitmapSource> Crop(
        IWICBitmapSource* source, 
        const RECT& region);
    static Microsoft::WRL::ComPtr<IWICBitmapSource> AutoCropNumber(IWICBitmapSource* source);
    
    // ==== 辅助方法 ====
    static std::vector<BYTE> GetPixelData(IWICBitmapSource* source);
    static void GetImageDimensions(IWICBitmapSource* source, UINT& width, UINT& height);

private:
    // ==== 内部工具类 ====
    class WICHelper {
    public:
        // RAII工具类
        class Factory {
        public:
            Factory();
            IWICImagingFactory2* Get() const;
            bool IsValid() const;
        private:
            Microsoft::WRL::ComPtr<IWICImagingFactory2> m_factory;
        };

        class BitmapLock {
        public:
            BitmapLock(IWICBitmap* bitmap, const WICRect& rect, DWORD flags);
            UINT GetStride() const;
            UINT GetBufferSize() const;
            BYTE* GetData() const;
            bool IsValid() const;
        private:
            Microsoft::WRL::ComPtr<IWICBitmapLock> m_lock;
            UINT m_stride;
            UINT m_bufferSize;
            BYTE* m_data;
            bool m_valid;
        };

        class ImageEncoder {
        public:
            ImageEncoder(const std::wstring& filePath);
            bool IsValid() const;
            bool SetSize(UINT width, UINT height);
            bool SetPixelFormat();
            bool WritePixels(UINT height, UINT stride, UINT bufferSize, BYTE* data);
            bool WriteSource(IWICBitmapSource* bitmap);
            bool Commit();
        private:
            Microsoft::WRL::ComPtr<IWICImagingFactory2> m_factory;
            Microsoft::WRL::ComPtr<IWICBitmapEncoder> m_encoder;
            Microsoft::WRL::ComPtr<IWICStream> m_stream;
            Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> m_frame;
            bool m_success;
        };
    };
}; 