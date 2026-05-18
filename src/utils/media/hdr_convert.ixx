module;

export module Utils.Media.HdrConvert;

import std;
import <d3d11.h>;
import <d3d11_4.h>;
import <wil/com.h>;

export namespace Utils::Media::HdrConvert {

// HDR 录制转换器：把 WGC 的 scRGB FP16 帧渲染成单张 P010 纹理的 Y/UV 两个 plane。
export struct ConverterContext {
  wil::com_ptr<ID3D11Device3> device3;
  wil::com_ptr<ID3D11Texture2D> p010_texture;
  wil::com_ptr<ID3D11Texture2D> source_texture;
  wil::com_ptr<ID3D11ShaderResourceView> source_srv;
  wil::com_ptr<ID3D11RenderTargetView1> luma_rtv;
  wil::com_ptr<ID3D11RenderTargetView1> chroma_rtv;
  wil::com_ptr<ID3D11VertexShader> fullscreen_vertex_shader;
  wil::com_ptr<ID3D11PixelShader> luma_pixel_shader;
  wil::com_ptr<ID3D11PixelShader> chroma_pixel_shader;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

export auto create_converter(ID3D11Device* device, std::uint32_t width, std::uint32_t height)
    -> std::expected<ConverterContext, std::string>;

export auto convert_frame(ConverterContext& converter, ID3D11DeviceContext* context,
                          ID3D11Texture2D* source_texture)
    -> std::expected<ID3D11Texture2D*, std::string>;

}  // namespace Utils::Media::HdrConvert
