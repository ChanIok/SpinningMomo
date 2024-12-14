#include "gui_manager.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <dxgi.h>
#include <cmath>
#include <utility>

// 声明ImGui的Win32消息处理函数
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

GuiManager::GuiManager()
    : hwnd(nullptr)
    , d3dDevice(nullptr)
    , d3dContext(nullptr)
    , mainRenderTargetView(nullptr)
    , swapChain(nullptr)
    , textureView(nullptr)
    , mirrorX(false)
    , mirrorY(false)
    , windowWidth(0)
    , windowHeight(0)
    , initialized(false)
{
}

GuiManager::~GuiManager()
{
    Cleanup();
}

bool GuiManager::Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context)
{
    if (initialized) return true;

    this->hwnd = hwnd;
    this->d3dDevice = device;
    this->d3dContext = context;

    // 创建渲染目标视图
    if (!CreateRenderTarget()) {
        OutputDebugStringW(L"Failed to create render target\n");
        return false;
    }

    // 初始化ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // 启用键盘控制
    io.IniFilename = nullptr;  // 禁用配置文件
    
    // 设置ImGui风格
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;  // 置为完全不透明

    // 初始化ImGui的Win32和DirectX11后端
    if (!ImGui_ImplWin32_Init(hwnd)) {
        OutputDebugStringW(L"Failed to initialize ImGui Win32\n");
        return false;
    }
    if (!ImGui_ImplDX11_Init(d3dDevice, d3dContext)) {
        OutputDebugStringW(L"Failed to initialize ImGui DirectX11\n");
        return false;
    }

    // 构建字体
    io.Fonts->AddFontDefault();
    if (!io.Fonts->Build()) {
        OutputDebugStringW(L"Failed to build font\n");
        return false;
    }

    // 设置初始窗口大小
    RECT rect;
    GetClientRect(hwnd, &rect);
    windowWidth = rect.right - rect.left;
    windowHeight = rect.bottom - rect.top;
    OutputDebugStringW(L"Window size initialized\n");

    initialized = true;
    OutputDebugStringW(L"GuiManager initialized successfully\n");
    return true;
}

void GuiManager::Render(ID3D11Texture2D* texture)
{
    if (!initialized) {
        OutputDebugStringW(L"GuiManager not initialized\n");
        return;
    }

    // 设置渲染目标
    d3dContext->OMSetRenderTargets(1, &mainRenderTargetView, nullptr);

    // 清除背景
    const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };  // 完全透明的背景
    d3dContext->ClearRenderTargetView(mainRenderTargetView, clear_color);

    // 开始新帧
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // 如果有捕获的纹理，显示它
    if (texture)
    {
        // 创建或更新纹理视图
        if (textureView) {
            textureView->Release();
            textureView = nullptr;
        }
        textureView = CreateTextureView(texture);

        if (textureView)
        {
            // 获取纹理尺寸
            D3D11_TEXTURE2D_DESC texDesc;
            texture->GetDesc(&texDesc);

            // 调整窗口大小以适应旋转后的纹理
            AdjustWindowForRotation(texDesc.Width, texDesc.Height);

            // 创建全屏窗口来显示纹理
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));
            ImGui::Begin("GameWindow", nullptr, 
                ImGuiWindowFlags_NoDecoration | 
                ImGuiWindowFlags_NoMove | 
                ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_NoBringToFrontOnFocus);

            // 计算旋转和镜像后的四个角点
            ImVec2 pos[4];
            float width = windowWidth;   // 使用窗口尺寸而不是纹理尺寸
            float height = windowHeight;

            // 90度旋转
            pos[0] = ImVec2(0, height);
            pos[1] = ImVec2(0, 0);
            pos[2] = ImVec2(width, 0);
            pos[3] = ImVec2(width, height);

            // UV坐标（考虑90度旋转和镜像）
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

            // 绘制纹理
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddImageQuad(
                reinterpret_cast<ImTextureID>(textureView),
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
                if (ImGui::MenuItem("Flip Vertical", nullptr, &mirrorX)) {}
                if (ImGui::MenuItem("Flip Horizontal", nullptr, &mirrorY)) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) PostQuitMessage(0);
                ImGui::EndPopup();
            }

            ImGui::End();
        }
    }

    // 渲染 ImGui
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // 呈现交换链
    HRESULT hr = swapChain->Present(1, 0);
    if (FAILED(hr)) {
        OutputDebugStringW(L"Failed to present swap chain\n");
    }
}

bool GuiManager::CreateRenderTarget()
{
    // 创建交换链
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

    IDXGIFactory* factory = nullptr;
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory))) {
        return false;
    }

    if (FAILED(factory->CreateSwapChain(d3dDevice, &sd, &swapChain))) {
        factory->Release();
        return false;
    }

    factory->Release();

    // 获取后备缓冲区
    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(hr)) {
        return false;
    }

    // 创建渲染目标视图
    hr = d3dDevice->CreateRenderTargetView(backBuffer, nullptr, &mainRenderTargetView);
    backBuffer->Release();
    
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

void GuiManager::CleanupRenderTarget()
{
    if (mainRenderTargetView) {
        mainRenderTargetView->Release();
        mainRenderTargetView = nullptr;
    }
    if (swapChain) {
        swapChain->Release();
        swapChain = nullptr;
    }
}

void GuiManager::Cleanup()
{
    if (initialized) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    if (textureView) {
        textureView->Release();
        textureView = nullptr;
    }

    CleanupRenderTarget();
    initialized = false;
}

void GuiManager::OnResize(UINT width, UINT height)
{
    windowWidth = width;
    windowHeight = height;
    CleanupRenderTarget();
    CreateRenderTarget();
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
    }
}

ID3D11ShaderResourceView* GuiManager::CreateTextureView(ID3D11Texture2D* texture)
{
    if (!texture)
        return nullptr;

    // 获取纹理描述
    D3D11_TEXTURE2D_DESC texDesc;
    texture->GetDesc(&texDesc);

    // 创着色器资源视图
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    ID3D11ShaderResourceView* srv = nullptr;
    HRESULT hr = d3dDevice->CreateShaderResourceView(texture, &srvDesc, &srv);
    if (FAILED(hr)) {
        return nullptr;
    }

    return srv;
}

void GuiManager::AdjustWindowForRotation(UINT textureWidth, UINT textureHeight)
{
    // 90度旋转后，宽高互换
    if (windowWidth != textureHeight || windowHeight != textureWidth) {
        SetSize(textureHeight, textureWidth);
    }
} 