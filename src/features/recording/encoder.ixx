module;

export module Features.Recording.Encoder;

import std;
import Features.Recording.State;
import Features.Recording.Types;
import <d3d11.h>;

export namespace Features::Recording::Encoder {

// 创建编码器
auto create_encoder(const std::filesystem::path& output_path, std::uint32_t width,
                    std::uint32_t height, std::uint32_t fps, std::uint32_t bitrate,
                    ID3D11Device* device, Features::Recording::Types::EncoderMode mode,
                    Features::Recording::Types::VideoCodec codec)
    -> std::expected<Features::Recording::State::EncoderContext, std::string>;

// 编码一帧
auto encode_frame(Features::Recording::State::EncoderContext& encoder, ID3D11DeviceContext* context,
                  ID3D11Texture2D* frame_texture, std::int64_t timestamp_100ns, std::uint32_t fps)
    -> std::expected<void, std::string>;

// 完成编码
auto finalize_encoder(Features::Recording::State::EncoderContext& encoder)
    -> std::expected<void, std::string>;

}  // namespace Features::Recording::Encoder
