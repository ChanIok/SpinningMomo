module;

export module Utils.Media.Encoder;

import std;
import Utils.Media.Encoder.Types;
import Utils.Media.Encoder.State;
import <audioclient.h>;
import <d3d11.h>;

export namespace Utils::Media::Encoder {

// 创建编码器
auto create_encoder(const Utils::Media::Encoder::Types::EncoderConfig& config, ID3D11Device* device,
                    WAVEFORMATEX* wave_format = nullptr)
    -> std::expected<Utils::Media::Encoder::State::EncoderContext, std::string>;

// 编码视频帧
auto encode_frame(Utils::Media::Encoder::State::EncoderContext& encoder,
                  ID3D11DeviceContext* context, ID3D11Texture2D* frame_texture,
                  std::int64_t timestamp_100ns, std::uint32_t fps)
    -> std::expected<void, std::string>;

// 编码音频样本
auto encode_audio(Utils::Media::Encoder::State::EncoderContext& encoder, const BYTE* audio_data,
                  UINT32 num_frames, UINT32 bytes_per_frame, std::int64_t timestamp_100ns)
    -> std::expected<void, std::string>;

// 完成编码
auto finalize_encoder(Utils::Media::Encoder::State::EncoderContext& encoder)
    -> std::expected<void, std::string>;

}  // namespace Utils::Media::Encoder
