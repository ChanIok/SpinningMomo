#include "gui_manager.h"

GuiManager::GuiManager()
    : hwnd(nullptr)
    , d3dDevice(nullptr)
    , d3dContext(nullptr)
    , mainRenderTargetView(nullptr)
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

    // 暂时返回true，后续实现具体逻辑
    return true;
}

void GuiManager::Render(ID3D11Texture2D* texture)
{
    if (!initialized) return;
    
    // 暂时为空，后续实现具体逻辑
}

bool GuiManager::CreateRenderTarget()
{
    // 暂时返回true，后续实现具体逻辑
    return true;
}

void GuiManager::CleanupRenderTarget()
{
    if (mainRenderTargetView) {
        mainRenderTargetView->Release();
        mainRenderTargetView = nullptr;
    }
}

void GuiManager::Cleanup()
{
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