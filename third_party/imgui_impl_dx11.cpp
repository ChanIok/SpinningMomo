#include "imgui.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>

// DirectX 数据
struct ImGui_ImplDX11_Data
{
    ID3D11Device*               pd3dDevice;
    ID3D11DeviceContext*        pd3dDeviceContext;
};

static ImGui_ImplDX11_Data* ImGui_ImplDX11_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplDX11_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

bool ImGui_ImplDX11_Init(ID3D11Device* device, ID3D11DeviceContext* device_context)
{
    ImGui_ImplDX11_Data* bd = new ImGui_ImplDX11_Data();
    ImGui::GetIO().BackendRendererUserData = (void*)bd;
    
    bd->pd3dDevice = device;
    bd->pd3dDeviceContext = device_context;
    return true;
}

void ImGui_ImplDX11_Shutdown()
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (bd)
    {
        delete bd;
        ImGui::GetIO().BackendRendererUserData = nullptr;
    }
}

void ImGui_ImplDX11_NewFrame()
{
}

void ImGui_ImplDX11_RenderDrawData(ImDrawData* draw_data)
{
}

void ImGui_ImplDX11_InvalidateDeviceObjects()
{
}

bool ImGui_ImplDX11_CreateDeviceObjects()
{
    return true;
} 