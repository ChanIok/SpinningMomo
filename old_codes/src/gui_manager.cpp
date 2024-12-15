#include "gui_manager.h"
#include <wincodec.h>
#include <algorithm>

GuiManager::GuiManager()
    : hwnd(nullptr)
    , initialized(false)
    , mirrorX(false)
    , mirrorY(false)
    , scaleRatio(1.0f)
    , baseTextureWidth(0)
    , baseTextureHeight(0)
{
}

GuiManager::~GuiManager()
{
    Cleanup();
}

bool GuiManager::Initialize(HWND hwnd)
{
    if (initialized) return true;
    this->hwnd = hwnd;

    // 创建 D3D 设备和交换链
    if (!CreateDeviceD3D()) {
        Cleanup();
        return false;
    }

    // 初始化 ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    
    // 设置中文字体
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\msyh.ttc", 18.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());

    if (!ImGui_ImplWin32_Init(hwnd)) {
        Cleanup();
        return false;
    }

    if (!ImGui_ImplDX11_Init(d3dDevice.get(), d3dContext.get())) {
        Cleanup();
        return false;
    }

    // 设置初始窗口大小
    RECT rect;
    GetClientRect(hwnd, &rect);
    windowWidth = rect.right - rect.left;
    windowHeight = rect.bottom - rect.top;

    initialized = true;
    return true;
}

bool GuiManager::CreateDeviceD3D()
{
    // 创建换链描述
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    // 创建设备和交换链
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION,
        &sd, swapChain.put(), d3dDevice.put(), &featureLevel, d3dContext.put()
    );

    if (FAILED(hr)) return false;

    return CreateRenderTarget();
}

bool GuiManager::CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer))))
        return false;

    HRESULT hr = d3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, mainRenderTargetView.put());
    pBackBuffer->Release();
    return SUCCEEDED(hr);
}

void GuiManager::CleanupRenderTarget()
{
    mainRenderTargetView = nullptr;
}

void GuiManager::Cleanup()
{
    CleanupRenderTarget();
    
    if (initialized)
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    screenshotTexture = nullptr;
    swapChain = nullptr;
    d3dContext = nullptr;
    d3dDevice = nullptr;
    
    initialized = false;
}

void GuiManager::OnResize(UINT width, UINT height)
{
    if (!initialized) return;

    CleanupRenderTarget();
    swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    CreateRenderTarget();
}

void GuiManager::OnMouseWheel(float delta)
{
    // 调整缩放比例
    float oldRatio = scaleRatio;
    scaleRatio = std::clamp(scaleRatio + delta * 0.1f, 0.1f, 10.0f);
    
    if (oldRatio != scaleRatio) {
        UpdateWindowSize();
    }
}

void GuiManager::SetPosition(int x, int y)
{
    if (hwnd) {
        SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}

void GuiManager::SetSize(int width, int height)
{
    if (hwnd) {
        SetWindowPos(hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
        OnResize(width, height);
        windowWidth = width;
        windowHeight = height;
    }
}

void GuiManager::AdjustWindowForRotation(UINT textureWidth, UINT textureHeight)
{
    // 首次设置基础尺寸
    if (baseTextureWidth == 0 || baseTextureHeight == 0) {
        baseTextureWidth = textureHeight;  // 注意：因为旋转90度，所以宽高互换
        baseTextureHeight = textureWidth;
        scaleRatio = 0.4f;  // 设置初始缩放为40%
    }
    
    UpdateWindowSize();
}

void GuiManager::UpdateWindowSize()
{
    // 计算缩放后的尺寸
    int newWidth = static_cast<int>(baseTextureWidth * scaleRatio);
    int newHeight = static_cast<int>(baseTextureHeight * scaleRatio);
    
    if (windowWidth != newWidth || windowHeight != newHeight) {
        SetSize(newWidth, newHeight);
    }
}

bool GuiManager::LoadTextureFromFile(const wchar_t* filename)
{
    // 创建 WIC 工厂
    winrt::com_ptr<IWICImagingFactory> wicFactory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&wicFactory)
    );
    if (FAILED(hr)) return false;

    // 加载图片
    winrt::com_ptr<IWICBitmapDecoder> wicDecoder;
    hr = wicFactory->CreateDecoderFromFilename(
        filename, nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, wicDecoder.put()
    );
    if (FAILED(hr)) return false;

    // 获取第一帧
    winrt::com_ptr<IWICBitmapFrameDecode> wicFrame;
    hr = wicDecoder->GetFrame(0, wicFrame.put());
    if (FAILED(hr)) return false;

    // 转换格式
    winrt::com_ptr<IWICFormatConverter> wicConverter;
    hr = wicFactory->CreateFormatConverter(wicConverter.put());
    if (FAILED(hr)) return false;

    hr = wicConverter->Initialize(
        wicFrame.get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0f,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr)) return false;

    // 获取图片大小
    UINT width, height;
    hr = wicConverter->GetSize(&width, &height);
    if (FAILED(hr)) return false;

    // 创建纹理
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    // 准备像素数据
    std::vector<BYTE> pixels(width * height * 4);
    hr = wicConverter->CopyPixels(
        nullptr, width * 4,
        static_cast<UINT>(pixels.size()),
        pixels.data()
    );
    if (FAILED(hr)) return false;

    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = pixels.data();
    subResource.SysMemPitch = width * 4;
    subResource.SysMemSlicePitch = 0;

    winrt::com_ptr<ID3D11Texture2D> texture;
    hr = d3dDevice->CreateTexture2D(&desc, &subResource, texture.put());
    if (FAILED(hr)) return false;

    // 创建着色器资源视图
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;

    hr = d3dDevice->CreateShaderResourceView(texture.get(), &srvDesc, screenshotTexture.put());
    if (FAILED(hr)) return false;

    // 调整窗口大小以适应旋转后的纹理
    AdjustWindowForRotation(width, height);

    return true;
}

void GuiManager::Render()
{
    if (!initialized) return;

    // 清除渲染目标
    const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    ID3D11RenderTargetView* rtv = mainRenderTargetView.get();
    d3dContext->OMSetRenderTargets(1, &rtv, nullptr);
    d3dContext->ClearRenderTargetView(mainRenderTargetView.get(), clear_color);

    // 开始 ImGui 帧
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // 创建全屏窗口
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Screenshot", nullptr, 
        ImGuiWindowFlags_NoDecoration | 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoSavedSettings);

    // 获取窗口大小
    ImVec2 windowSize = ImGui::GetContentRegionAvail();
    
    // 如果有截图纹理，显示它
    if (screenshotTexture)
    {
        // 计算旋转和缩放后的尺寸
        float aspectRatio = (float)baseTextureWidth / baseTextureHeight;
        ImVec2 imageSize;
        if (windowSize.x / aspectRatio <= windowSize.y) {
            imageSize.x = windowSize.x * scaleRatio;
            imageSize.y = windowSize.x / aspectRatio * scaleRatio;
        } else {
            imageSize.y = windowSize.y * scaleRatio;
            imageSize.x = windowSize.y * aspectRatio * scaleRatio;
        }

        // 居中显示图片
        float offsetX = (windowSize.x - imageSize.x) * 0.5f;
        float offsetY = (windowSize.y - imageSize.y) * 0.5f;

        // 计算四个角点的位置
        ImVec2 pos[4];
        pos[0] = ImVec2(offsetX, offsetY + imageSize.y);  // 左下
        pos[1] = ImVec2(offsetX, offsetY);                // 左上
        pos[2] = ImVec2(offsetX + imageSize.x, offsetY);  // ���上
        pos[3] = ImVec2(offsetX + imageSize.x, offsetY + imageSize.y);  // 右下

        // 计算UV坐标（考虑镜像）
        ImVec2 uv[4];
        if (mirrorX) {
            uv[0] = ImVec2(0, 1);
            uv[1] = ImVec2(1, 1);
            uv[2] = ImVec2(1, 0);
            uv[3] = ImVec2(0, 0);
        } else {
            uv[0] = ImVec2(1, 1);
            uv[1] = ImVec2(0, 1);
            uv[2] = ImVec2(0, 0);
            uv[3] = ImVec2(1, 0);
        }

        if (mirrorY) {
            std::swap(uv[0], uv[3]);
            std::swap(uv[1], uv[2]);
        }

        // 绘制图片
        ImGui::GetWindowDrawList()->AddImageQuad(
            reinterpret_cast<ImTextureID>(screenshotTexture.get()),
            pos[0], pos[1], pos[2], pos[3],
            uv[0], uv[1], uv[2], uv[3]
        );

        // 处理右键菜单
        if (ImGui::IsMouseClicked(1) && ImGui::IsWindowHovered())
        {
            ImGui::OpenPopup("ContextMenu");
        }

        if (ImGui::BeginPopup("ContextMenu"))
        {
            if (ImGui::MenuItem("垂直翻转", nullptr, &mirrorX)) {}
            if (ImGui::MenuItem("水平翻转", nullptr, &mirrorY)) {}
            ImGui::Separator();
            if (ImGui::MenuItem("退出")) PostQuitMessage(0);
            ImGui::EndPopup();
        }
    }

    ImGui::End();

    // 渲染 ImGui
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // 显示渲染结果
    swapChain->Present(1, 0);
} 