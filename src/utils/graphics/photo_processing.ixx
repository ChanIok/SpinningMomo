module;

export module Utils.Graphics.PhotoProcessing;

import std;
import <d3d11.h>;
import <wil/com.h>;

namespace Utils::Graphics::PhotoProcessing {

// GPU 端长曝光累积器：双缓冲 ping-pong + 逐帧加权混合
export struct AverageAccumulator {
  wil::com_ptr<ID3D11Texture2D> current_average;
  wil::com_ptr<ID3D11Texture2D> scratch;
  // 视图跟随双缓冲纹理一起交换，避免每帧重建 SRV/RTV。
  wil::com_ptr<ID3D11ShaderResourceView> current_average_srv;
  wil::com_ptr<ID3D11ShaderResourceView> scratch_srv;
  wil::com_ptr<ID3D11RenderTargetView> current_average_rtv;
  wil::com_ptr<ID3D11RenderTargetView> scratch_rtv;
  wil::com_ptr<ID3D11VertexShader> fullscreen_vertex_shader;
  wil::com_ptr<ID3D11PixelShader> average_pixel_shader;
  wil::com_ptr<ID3D11SamplerState> sampler;
  wil::com_ptr<ID3D11Buffer> constants_buffer;
  UINT width = 0;
  UINT height = 0;
  DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
  std::uint32_t frame_count = 0;
};

export auto initialize_average_accumulator(ID3D11Texture2D* first_frame)
    -> std::expected<AverageAccumulator, std::string>;

export auto accumulate_average_frame(AverageAccumulator& accumulator, ID3D11Texture2D* frame)
    -> std::expected<void, std::string>;

}  // namespace Utils::Graphics::PhotoProcessing
