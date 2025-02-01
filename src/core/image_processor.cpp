#include "image_processor.hpp"
#include <stdexcept>
#include <algorithm>

// ==== WICHelper::Factory 实现 ====
ImageProcessor::WICHelper::Factory::Factory() : m_factory(nullptr) {
    CoCreateInstance(
        CLSID_WICImagingFactory2,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&m_factory)
    );
}

IWICImagingFactory2* ImageProcessor::WICHelper::Factory::Get() const {
    return m_factory.Get();
}

bool ImageProcessor::WICHelper::Factory::IsValid() const {
    return m_factory != nullptr;
}

// ==== WICHelper::BitmapLock 实现 ====
ImageProcessor::WICHelper::BitmapLock::BitmapLock(IWICBitmap* bitmap, const WICRect& rect, DWORD flags)
    : m_stride(0), m_bufferSize(0), m_data(nullptr), m_valid(false) {
    if (!bitmap) return;

    HRESULT hr = bitmap->Lock(&rect, flags, &m_lock);
    if (FAILED(hr)) return;

    hr = m_lock->GetStride(&m_stride);
    if (FAILED(hr)) return;

    hr = m_lock->GetDataPointer(&m_bufferSize, &m_data);
    if (FAILED(hr)) return;

    m_valid = true;
}

UINT ImageProcessor::WICHelper::BitmapLock::GetStride() const {
    return m_stride;
}

UINT ImageProcessor::WICHelper::BitmapLock::GetBufferSize() const {
    return m_bufferSize;
}

BYTE* ImageProcessor::WICHelper::BitmapLock::GetData() const {
    return m_data;
}

bool ImageProcessor::WICHelper::BitmapLock::IsValid() const {
    return m_valid;
}

// ==== WICHelper::ImageEncoder 实现 ====
ImageProcessor::WICHelper::ImageEncoder::ImageEncoder(const std::wstring& filePath) 
    : m_success(false) {
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

bool ImageProcessor::WICHelper::ImageEncoder::IsValid() const {
    return m_success;
}

bool ImageProcessor::WICHelper::ImageEncoder::SetSize(UINT width, UINT height) {
    return SUCCEEDED(m_frame->SetSize(width, height));
}

bool ImageProcessor::WICHelper::ImageEncoder::SetPixelFormat() {
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    return SUCCEEDED(m_frame->SetPixelFormat(&format));
}

bool ImageProcessor::WICHelper::ImageEncoder::WritePixels(UINT height, UINT stride, UINT bufferSize, BYTE* data) {
    return SUCCEEDED(m_frame->WritePixels(height, stride, bufferSize, data));
}

bool ImageProcessor::WICHelper::ImageEncoder::WriteSource(IWICBitmapSource* bitmap) {
    if (!bitmap || !m_frame) return false;
    
    // 获取位图大小
    UINT width = 0, height = 0;
    HRESULT hr = bitmap->GetSize(&width, &height);
    if (FAILED(hr)) return false;

    // 设置帧大小
    hr = m_frame->SetSize(width, height);
    if (FAILED(hr)) return false;

    // 设置像素格式
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    hr = m_frame->SetPixelFormat(&format);
    if (FAILED(hr)) return false;

    // 写入图像数据
    return SUCCEEDED(m_frame->WriteSource(bitmap, nullptr));
}

bool ImageProcessor::WICHelper::ImageEncoder::Commit() {
    if (FAILED(m_frame->Commit())) return false;
    return SUCCEEDED(m_encoder->Commit());
}

// ==== ImageProcessor 辅助方法实现 ====
void ImageProcessor::GetImageDimensions(IWICBitmapSource* source, UINT& width, UINT& height) {
    if (!source) {
        width = height = 0;
        return;
    }
    source->GetSize(&width, &height);
}

std::vector<BYTE> ImageProcessor::GetPixelData(IWICBitmapSource* source) {
    if (!source) return {};

    UINT width = 0, height = 0;
    GetImageDimensions(source, width, height);
    if (width == 0 || height == 0) return {};

    UINT stride = ((width * 8 + 31) / 32) * 4; // 8bpp的stride计算
    std::vector<BYTE> buffer(stride * height);
    
    HRESULT hr = source->CopyPixels(nullptr, stride, stride * height, buffer.data());
    if (FAILED(hr)) return {};

    return buffer;
}

// ==== 基础图像操作实现 ====
Microsoft::WRL::ComPtr<IWICBitmapSource> ImageProcessor::ConvertToGrayscale(IWICBitmapSource* source) {
    if (!source) return nullptr;

    // 创建WIC工厂
    WICHelper::Factory factory;
    if (!factory.IsValid()) return nullptr;

    // 创建颜色转换器
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    HRESULT hr = factory.Get()->CreateFormatConverter(&converter);
    if (FAILED(hr)) return nullptr;

    // 初始化转换器为灰度格式
    hr = converter->Initialize(
        source,                          // 输入源
        GUID_WICPixelFormat8bppGray,    // 目标格式：8位灰度
        WICBitmapDitherTypeNone,        // 不使用抖动
        nullptr,                         // 不使用调色板
        0.0f,                           // 透明度阈值
        WICBitmapPaletteTypeCustom      // 自定义调色板
    );
    if (FAILED(hr)) return nullptr;

    // 返回结果
    Microsoft::WRL::ComPtr<IWICBitmapSource> result;
    hr = converter.As(&result);
    if (FAILED(hr)) return nullptr;

    return result;
}

Microsoft::WRL::ComPtr<IWICBitmapSource> ImageProcessor::Resize(
    IWICBitmapSource* source,
    UINT width,
    UINT height,
    WICBitmapInterpolationMode mode) {
    
    if (!source) return nullptr;

    // 创建WIC工厂
    WICHelper::Factory factory;
    if (!factory.IsValid()) return nullptr;

    // 创建缩放器
    Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
    HRESULT hr = factory.Get()->CreateBitmapScaler(&scaler);
    if (FAILED(hr)) return nullptr;

    // 初始化缩放器
    hr = scaler->Initialize(
        source,
        width,
        height,
        mode
    );
    if (FAILED(hr)) return nullptr;

    // 返回结果
    Microsoft::WRL::ComPtr<IWICBitmapSource> result;
    hr = scaler.As(&result);
    if (FAILED(hr)) return nullptr;

    return result;
}

Microsoft::WRL::ComPtr<IWICBitmapSource> ImageProcessor::ResizeByLongEdge(
    IWICBitmapSource* source,
    UINT longEdgeLength,
    WICBitmapInterpolationMode mode) {
    
    if (!source) return nullptr;

    // 获取原始尺寸
    UINT originalWidth = 0, originalHeight = 0;
    GetImageDimensions(source, originalWidth, originalHeight);
    if (originalWidth == 0 || originalHeight == 0) return nullptr;

    // 计算目标尺寸
    UINT targetWidth, targetHeight;
    if (originalWidth > originalHeight) {
        targetWidth = longEdgeLength;
        targetHeight = static_cast<UINT>((longEdgeLength * originalHeight) / static_cast<float>(originalWidth));
    } else {
        targetHeight = longEdgeLength;
        targetWidth = static_cast<UINT>((longEdgeLength * originalWidth) / static_cast<float>(originalHeight));
    }

    // 使用Resize方法
    return Resize(source, targetWidth, targetHeight, mode);
}

// ==== 图像处理操作实现 ====
Microsoft::WRL::ComPtr<IWICBitmapSource> ImageProcessor::Binarize(IWICBitmapSource* source, BYTE threshold) {
    if (!source) return nullptr;

    // 获取源图像尺寸
    UINT width = 0, height = 0;
    GetImageDimensions(source, width, height);
    if (width == 0 || height == 0) return nullptr;

    // 创建WIC工厂
    WICHelper::Factory factory;
    if (!factory.IsValid()) return nullptr;

    // 创建新的位图
    Microsoft::WRL::ComPtr<IWICBitmap> binaryBitmap;
    HRESULT hr = factory.Get()->CreateBitmap(
        width, height, 
        GUID_WICPixelFormat8bppGray, 
        WICBitmapCacheOnLoad, 
        &binaryBitmap);
    if (FAILED(hr)) return nullptr;

    // 读取源数据
    std::vector<BYTE> sourceBuffer = GetPixelData(source);
    if (sourceBuffer.empty()) return nullptr;

    // 锁定目标位图进行写入
    WICRect lockRect = { 0, 0, static_cast<INT>(width), static_cast<INT>(height) };
    WICHelper::BitmapLock lock(binaryBitmap.Get(), lockRect, WICBitmapLockWrite);
    if (!lock.IsValid()) return nullptr;

    // 执行二值化
    BYTE* targetData = lock.GetData();
    UINT targetStride = lock.GetStride();
    UINT sourceStride = ((width * 8 + 31) / 32) * 4; // 8bpp的stride计算

    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            BYTE sourceValue = sourceBuffer[y * sourceStride + x];
            targetData[y * targetStride + x] = (sourceValue > threshold) ? 255 : 0;
        }
    }

    // 返回结果
    Microsoft::WRL::ComPtr<IWICBitmapSource> result;
    hr = binaryBitmap.As(&result);
    if (FAILED(hr)) return nullptr;

    return result;
}

Microsoft::WRL::ComPtr<IWICBitmapSource> ImageProcessor::Trinarize(
    IWICBitmapSource* source, 
    BYTE lowerThreshold, 
    BYTE upperThreshold) {
    
    if (!source) return nullptr;

    // 获取源图像尺寸
    UINT width = 0, height = 0;
    GetImageDimensions(source, width, height);
    if (width == 0 || height == 0) return nullptr;

    // 创建WIC工厂
    WICHelper::Factory factory;
    if (!factory.IsValid()) return nullptr;

    // 创建新的位图
    Microsoft::WRL::ComPtr<IWICBitmap> trinaryBitmap;
    HRESULT hr = factory.Get()->CreateBitmap(
        width, height, 
        GUID_WICPixelFormat8bppGray, 
        WICBitmapCacheOnLoad, 
        &trinaryBitmap);
    if (FAILED(hr)) return nullptr;

    // 读取源数据
    std::vector<BYTE> sourceBuffer = GetPixelData(source);
    if (sourceBuffer.empty()) return nullptr;

    // 锁定目标位图进行写入
    WICRect lockRect = { 0, 0, static_cast<INT>(width), static_cast<INT>(height) };
    WICHelper::BitmapLock lock(trinaryBitmap.Get(), lockRect, WICBitmapLockWrite);
    if (!lock.IsValid()) return nullptr;

    // 执行三值化
    BYTE* targetData = lock.GetData();
    UINT targetStride = lock.GetStride();
    UINT sourceStride = ((width * 8 + 31) / 32) * 4; // 8bpp的stride计算

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
    Microsoft::WRL::ComPtr<IWICBitmapSource> result;
    hr = trinaryBitmap.As(&result);
    if (FAILED(hr)) return nullptr;

    return result;
}

// ==== 格式转换和保存操作实现 ====
Microsoft::WRL::ComPtr<IWICBitmapSource> ImageProcessor::TextureToWICBitmap(ID3D11Texture2D* texture) {
    if (!texture) return nullptr;

    // 获取纹理描述
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);

    // 获取设备和上下文
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    texture->GetDevice(&device);
    if (!device) return nullptr;

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);
    if (!context) return nullptr;

    // 创建暂存纹理
    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.ArraySize = 1;
    stagingDesc.MipLevels = 1;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;
    HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr)) return nullptr;

    // 复制纹理数据
    context->CopyResource(stagingTexture.Get(), texture);

    // 创建WIC工厂
    WICHelper::Factory factory;
    if (!factory.IsValid()) return nullptr;

    // 创建WIC位图
    Microsoft::WRL::ComPtr<IWICBitmap> wicBitmap;
    hr = factory.Get()->CreateBitmap(
        desc.Width, desc.Height,
        GUID_WICPixelFormat32bppBGRA,
        WICBitmapCacheOnLoad,
        &wicBitmap);
    if (FAILED(hr)) return nullptr;

    // 映射暂存纹理
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return nullptr;

    // 锁定WIC位图进行写入
    WICRect rect = { 0, 0, static_cast<INT>(desc.Width), static_cast<INT>(desc.Height) };
    WICHelper::BitmapLock lock(wicBitmap.Get(), rect, WICBitmapLockWrite);
    if (!lock.IsValid()) {
        context->Unmap(stagingTexture.Get(), 0);
        return nullptr;
    }

    // 复制数据
    BYTE* targetData = lock.GetData();
    UINT targetStride = lock.GetStride();
    BYTE* sourceData = static_cast<BYTE*>(mapped.pData);
    UINT sourceStride = mapped.RowPitch;

    for (UINT i = 0; i < desc.Height; ++i) {
        memcpy(
            targetData + i * targetStride,
            sourceData + i * sourceStride,
            (std::min)(targetStride, sourceStride)
        );
    }

    // 解除映射
    context->Unmap(stagingTexture.Get(), 0);

    // 返回结果
    Microsoft::WRL::ComPtr<IWICBitmapSource> result;
    hr = wicBitmap.As(&result);
    if (FAILED(hr)) return nullptr;

    return result;
}

bool ImageProcessor::SaveToFile(IWICBitmapSource* bitmap, const std::wstring& filePath) {
    if (!bitmap) return false;

    // 创建编码器并保存
    WICHelper::ImageEncoder encoder(filePath);
    if (!encoder.IsValid()) return false;

    return encoder.WriteSource(bitmap) && encoder.Commit();
}

bool ImageProcessor::SaveToJpegFile(IWICBitmapSource* bitmap, const std::wstring& filePath, float quality) {
    if (!bitmap) return false;

    // 创建WIC工厂
    WICHelper::Factory factory;
    if (!factory.IsValid()) return nullptr;

    // 创建编码器
    Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
    HRESULT hr = factory.Get()->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder);
    if (FAILED(hr)) return false;

    // 创建流
    Microsoft::WRL::ComPtr<IWICStream> stream;
    hr = factory.Get()->CreateStream(&stream);
    if (FAILED(hr)) return false;

    // 初始化流
    hr = stream->InitializeFromFilename(filePath.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) return false;

    // 初始化编码器
    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) return false;

    // 创建新帧
    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
    Microsoft::WRL::ComPtr<IPropertyBag2> propertyBag;
    hr = encoder->CreateNewFrame(&frame, &propertyBag);
    if (FAILED(hr)) return false;

    // 设置JPEG质量
    PROPBAG2 option = { 0 };
    option.pstrName = L"ImageQuality";
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_R4;
    value.fltVal = quality;
    hr = propertyBag->Write(1, &option, &value);
    if (FAILED(hr)) return false;

    // 初始化帧
    hr = frame->Initialize(propertyBag.Get());
    if (FAILED(hr)) return false;

    // 获取图像尺寸
    UINT width = 0, height = 0;
    GetImageDimensions(bitmap, width, height);

    // 设置帧大小
    hr = frame->SetSize(width, height);
    if (FAILED(hr)) return false;

    // 设置像素格式
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr)) return false;

    // 写入图像数据
    hr = frame->WriteSource(bitmap, nullptr);
    if (FAILED(hr)) return false;

    // 提交帧和编码器
    hr = frame->Commit();
    if (FAILED(hr)) return false;

    hr = encoder->Commit();
    if (FAILED(hr)) return false;

    return true;
}

// ==== 区域处理操作实现 ====
Microsoft::WRL::ComPtr<IWICBitmapSource> ImageProcessor::Crop(
    IWICBitmapSource* source, 
    const RECT& region) {
    
    if (!source) return nullptr;

    // 获取源图像尺寸
    UINT width = 0, height = 0;
    GetImageDimensions(source, width, height);
    if (width == 0 || height == 0) return nullptr;

    // 验证裁剪区域是否有效
    if (region.left < 0 || region.top < 0 ||
        region.right > static_cast<int>(width) ||
        region.bottom > static_cast<int>(height) ||
        region.left >= region.right ||
        region.top >= region.bottom) {
        return nullptr;
    }

    // 创建WIC工厂
    WICHelper::Factory factory;
    if (!factory.IsValid()) return nullptr;

    // 创建裁剪器
    Microsoft::WRL::ComPtr<IWICBitmapClipper> clipper;
    HRESULT hr = factory.Get()->CreateBitmapClipper(&clipper);
    if (FAILED(hr)) return nullptr;

    // 设置裁剪区域
    WICRect rect = {
        region.left,
        region.top,
        region.right - region.left,
        region.bottom - region.top
    };

    // 初始化裁剪器
    hr = clipper->Initialize(source, &rect);
    if (FAILED(hr)) return nullptr;

    // 返回结果
    Microsoft::WRL::ComPtr<IWICBitmapSource> result;
    hr = clipper.As(&result);
    if (FAILED(hr)) return nullptr;

    return result;
}

Microsoft::WRL::ComPtr<IWICBitmapSource> ImageProcessor::AutoCropNumber(IWICBitmapSource* source) {
    if (!source) return nullptr;

    // 获取源图像尺寸和数据
    UINT width = 0, height = 0;
    GetImageDimensions(source, width, height);
    if (width == 0 || height == 0) return nullptr;

    std::vector<BYTE> buffer = GetPixelData(source);
    if (buffer.empty()) return nullptr;

    // 计算水平投影
    std::vector<int> hProjection(height, 0);
    UINT stride = ((width * 8 + 31) / 32) * 4; // 8bpp的stride计算
    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            if (buffer[y * stride + x] == 255) {  // 白色像素
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
            if (buffer[y * stride + x] == 255) {  // 白色像素
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
        return nullptr;
    }

    // 使用Crop方法裁剪图像
    RECT cropRect = {
        left,
        newTop,
        right + 1,  // right是inclusive的
        newBottom
    };

    return Crop(source, cropRect);
}

Microsoft::WRL::ComPtr<IWICBitmapSource> ImageProcessor::LoadFromFile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return nullptr;

    // 创建WIC工厂
    WICHelper::Factory factory;
    if (!factory.IsValid()) return nullptr;

    // 创建解码器
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory.Get()->CreateDecoderFromFilename(
        path.wstring().c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder
    );
    if (FAILED(hr)) return nullptr;

    // 获取第一帧
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return nullptr;

    // 创建格式转换器
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = factory.Get()->CreateFormatConverter(&converter);
    if (FAILED(hr)) return nullptr;

    // 初始化转换器为32位BGRA格式
    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr)) return nullptr;

    // 返回转换后的位图
    Microsoft::WRL::ComPtr<IWICBitmapSource> result;
    hr = converter.As(&result);
    if (FAILED(hr)) return nullptr;

    return result;
} 