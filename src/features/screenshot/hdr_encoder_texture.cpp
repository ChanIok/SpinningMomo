module;

#include <d3d11.h>
#include <wil/com.h>
#include <wil/result.h>
#include <windows.h>

module Features.Screenshot.HdrEncoder;

import std;

namespace Features::Screenshot::HdrEncoder {

// 把 DXGI R16G16B16A16_FLOAT 纹理拷到 staging，再读到 CPU。
// 返回的 vector 按“宽度方向连续”的 RGBA 排列（无 RowPitch 空洞），方便后续按像素顺序处理。
auto map_texture_to_tight_rgba_half(ID3D11Texture2D* texture)
    -> std::expected<std::tuple<std::vector<std::uint16_t>, std::uint32_t, std::uint32_t>,
                     std::string> {
  if (!texture) {
    return std::unexpected("Texture cannot be null");
  }

  try {
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    // HDR 捕获路径约定为 64 位半精度浮点 RGBA，否则无法按 half 解释像素。
    if (desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT) {
      return std::unexpected(
          std::format("Unexpected HDR texture format: {}", std::to_underlying(desc.Format)));
    }
    if (desc.Width == 0 || desc.Height == 0) {
      return std::unexpected("HDR texture has empty dimensions");
    }

    wil::com_ptr<ID3D11Device> device;
    texture->GetDevice(device.put());
    THROW_HR_IF_NULL(E_POINTER, device);

    wil::com_ptr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.put());
    THROW_HR_IF_NULL(E_POINTER, context);

    // 可读回读的 staging，与源格式一致，Map 后可在 CPU 侧读像素。
    D3D11_TEXTURE2D_DESC staging_desc = desc;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.BindFlags = 0;
    staging_desc.MiscFlags = 0;
    staging_desc.ArraySize = 1;
    staging_desc.MipLevels = 1;

    wil::com_ptr<ID3D11Texture2D> staging_texture;
    THROW_IF_FAILED(device->CreateTexture2D(&staging_desc, nullptr, staging_texture.put()));
    context->CopyResource(staging_texture.get(), texture);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    THROW_IF_FAILED(context->Map(staging_texture.get(), 0, D3D11_MAP_READ, 0, &mapped));
    auto unmap_on_exit = wil::scope_exit([&] { context->Unmap(staging_texture.get(), 0); });

    constexpr std::uint32_t kChannelsPerPixel = 4;
    constexpr std::uint32_t kBytesPerSourcePixel = 8;
    std::uint64_t pixel_count = static_cast<std::uint64_t>(desc.Width) * desc.Height;
    if (pixel_count > std::numeric_limits<std::size_t>::max() / kChannelsPerPixel) {
      return std::unexpected("HDR texture is too large");
    }

    // 每通道 16 bit half，共 8 byte/像素；最终缓存在 vector 里按线性的 width×height×4。
    std::vector<std::uint16_t> pixels(static_cast<std::size_t>(pixel_count) * kChannelsPerPixel);

    // mapped.RowPitch 可能大于 width×8（对齐）；逐行拷贝消除 padding。
    for (std::uint32_t y = 0; y < desc.Height; ++y) {
      auto* src_row = static_cast<const std::byte*>(mapped.pData) +
                      static_cast<std::size_t>(mapped.RowPitch) * y;
      auto* dst_row = pixels.data() + static_cast<std::size_t>(desc.Width) * y * kChannelsPerPixel;
      for (std::uint32_t x = 0; x < desc.Width; ++x) {
        auto* src_pixel = reinterpret_cast<const std::uint16_t*>(
            src_row + static_cast<std::size_t>(x) * kBytesPerSourcePixel);
        auto* dst_pixel = dst_row + static_cast<std::size_t>(x) * kChannelsPerPixel;
        dst_pixel[0] = src_pixel[0];
        dst_pixel[1] = src_pixel[1];
        dst_pixel[2] = src_pixel[2];
        dst_pixel[3] = src_pixel[3];
      }
    }

    return std::make_tuple(std::move(pixels), desc.Width, desc.Height);
  } catch (const wil::ResultException& e) {
    return std::unexpected(std::format("Failed to map HDR texture: {}", e.what()));
  }
}

}  // namespace Features::Screenshot::HdrEncoder
