#pragma once

#include "vendor/std.hpp"
#include "vendor/wil.hpp"
#include "vendor/windows/d3d11.hpp"
#include "vendor/windows/d3d11_4.hpp"

namespace utils::media::video_convert {

// 视频格式转换目标纹理及其 PlaneSlice 对应的亮度/色度 RTV
struct ConversionTarget {
  wil::com_ptr<ID3D11Texture2D> texture;
  wil::com_ptr<ID3D11RenderTargetView1> luma;
  wil::com_ptr<ID3D11RenderTargetView1> chroma;
};

// 视频格式转换器上下文：保存着色器、设备接口与尺寸配置
struct ConverterContext {
  wil::com_ptr<ID3D11Device3> device3;
  wil::com_ptr<ID3D11Texture2D> source_texture;
  wil::com_ptr<ID3D11ShaderResourceView> source_srv;
  wil::com_ptr<ID3D11VertexShader> fullscreen_vertex_shader;
  wil::com_ptr<ID3D11PixelShader> luma_pixel_shader;
  wil::com_ptr<ID3D11PixelShader> chroma_pixel_shader;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  bool hdr = false;
};

// 初始化视频格式转换器：编译全屏顶点与亮度/色度像素着色器
auto create_converter(ID3D11Device* device, std::uint32_t width, std::uint32_t height, bool hdr)
    -> std::expected<ConverterContext, std::string>;

// 创建单份两平面（NV12 或 P010）目标渲染纹理及独立平面 RTV
auto create_target(ConverterContext& converter) -> std::expected<ConversionTarget, std::string>;

// 执行帧格式转换：绑定源纹理 SRV → 渲染亮度 Pass → 渲染色度 Pass → 清理渲染管线绑定
auto convert_frame(ConverterContext& converter, ID3D11DeviceContext* context,
                   ID3D11Texture2D* source, ConversionTarget& target)
    -> std::expected<void, std::string>;

}  // namespace utils::media::video_convert
