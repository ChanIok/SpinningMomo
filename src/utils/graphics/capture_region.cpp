module;

#include <d3d11.h>

module Utils.Graphics.CaptureRegion;

import std;
import <windows.h>;

namespace Utils::Graphics::CaptureRegion {

auto calculate_client_crop_region(HWND target_window) -> std::expected<CropRegion, std::string> {
  if (!target_window || !IsWindow(target_window)) {
    return std::unexpected("Target window is invalid");
  }

  RECT window_rect{};
  if (!GetWindowRect(target_window, &window_rect)) {
    return std::unexpected("Failed to get window rect");
  }

  RECT client_rect{};
  if (!GetClientRect(target_window, &client_rect)) {
    return std::unexpected("Failed to get client rect");
  }

  POINT client_origin{0, 0};
  if (!ClientToScreen(target_window, &client_origin)) {
    return std::unexpected("Failed to convert client origin to screen coordinates");
  }

  int left = client_origin.x - window_rect.left;
  int top = client_origin.y - window_rect.top;
  int width = client_rect.right - client_rect.left;
  int height = client_rect.bottom - client_rect.top;

  if (left < 0 || top < 0 || width <= 0 || height <= 0) {
    return std::unexpected("Computed client crop region is invalid");
  }

  return CropRegion{
      .left = static_cast<UINT>(left),
      .top = static_cast<UINT>(top),
      .width = static_cast<UINT>(width),
      .height = static_cast<UINT>(height),
  };
}

auto crop_texture_to_region(ID3D11Device* device, ID3D11DeviceContext* context,
                            ID3D11Texture2D* source_texture, const CropRegion& region,
                            wil::com_ptr<ID3D11Texture2D>& output_texture)
    -> std::expected<ID3D11Texture2D*, std::string> {
  if (!device || !context || !source_texture) {
    return std::unexpected("Invalid D3D resources for texture crop");
  }

  if (region.width == 0 || region.height == 0) {
    return std::unexpected("Crop region size is invalid");
  }

  D3D11_TEXTURE2D_DESC source_desc{};
  source_texture->GetDesc(&source_desc);

  if (region.left >= source_desc.Width || region.top >= source_desc.Height) {
    return std::unexpected("Crop region origin is out of source bounds");
  }

  UINT right = std::min(region.left + region.width, source_desc.Width);
  UINT bottom = std::min(region.top + region.height, source_desc.Height);
  UINT cropped_width = right - region.left;
  UINT cropped_height = bottom - region.top;

  if (cropped_width == 0 || cropped_height == 0) {
    return std::unexpected("Crop region is empty after clamping");
  }

  bool need_recreate = !output_texture;
  if (!need_recreate) {
    D3D11_TEXTURE2D_DESC output_desc{};
    output_texture->GetDesc(&output_desc);
    need_recreate = output_desc.Width != cropped_width || output_desc.Height != cropped_height ||
                    output_desc.Format != source_desc.Format;
  }

  if (need_recreate) {
    D3D11_TEXTURE2D_DESC target_desc = source_desc;
    target_desc.Width = cropped_width;
    target_desc.Height = cropped_height;
    target_desc.Usage = D3D11_USAGE_DEFAULT;
    target_desc.BindFlags = 0;
    target_desc.CPUAccessFlags = 0;
    target_desc.MiscFlags = 0;

    output_texture = nullptr;
    if (FAILED(device->CreateTexture2D(&target_desc, nullptr, output_texture.put()))) {
      return std::unexpected("Failed to create cropped output texture");
    }
  }

  D3D11_BOX source_box{};
  source_box.left = region.left;
  source_box.top = region.top;
  source_box.right = right;
  source_box.bottom = bottom;
  source_box.front = 0;
  source_box.back = 1;

  context->CopySubresourceRegion(output_texture.get(), 0, 0, 0, 0, source_texture, 0, &source_box);

  return output_texture.get();
}

}  // namespace Utils::Graphics::CaptureRegion
