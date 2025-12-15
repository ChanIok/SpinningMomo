module;

#include <codecvt>
#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wil/com.h>

export module Features.Recording.Encoder;

import std;
import Features.Recording.State;
import Features.Recording.Types;

export namespace Features::Recording::Encoder {

// 创建编码器
auto create_encoder(const std::filesystem::path& output_path, uint32_t width, uint32_t height,
                    uint32_t fps, uint32_t bitrate)
    -> std::expected<Features::Recording::State::EncoderContext, std::string>;

// 编码一帧
auto encode_frame(Features::Recording::State::EncoderContext& encoder, ID3D11DeviceContext* context,
                  ID3D11Texture2D* frame_texture, int64_t timestamp_100ns)
    -> std::expected<void, std::string>;

// 完成编码
auto finalize_encoder(Features::Recording::State::EncoderContext& encoder)
    -> std::expected<void, std::string>;

}  // namespace Features::Recording::Encoder
