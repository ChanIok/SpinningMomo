#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/audioclient.hpp"
#include "vendor/windows/d3d11.hpp"

#include "utils/media/state.hpp"
#include "utils/media/types.hpp"

namespace utils::media::encoder {

// 创建编码器
auto create_encoder(const utils::media::encoder::EncoderConfig& config, ID3D11Device* device,
                    WAVEFORMATEX* wave_format = nullptr)
    -> std::expected<utils::media::encoder::EncoderContext, std::string>;

// 编码视频帧
auto encode_frame(utils::media::encoder::EncoderContext& encoder, ID3D11DeviceContext* context,
                  ID3D11Texture2D* frame_texture, std::int64_t timestamp_100ns, std::uint32_t fps)
    -> std::expected<void, std::string>;

// 编码音频样本
auto encode_audio(utils::media::encoder::EncoderContext& encoder, const BYTE* audio_data,
                  UINT32 num_frames, UINT32 bytes_per_frame, std::int64_t timestamp_100ns)
    -> std::expected<void, std::string>;

// 完成编码
auto finalize_encoder(utils::media::encoder::EncoderContext& encoder)
    -> std::expected<void, std::string>;

}  // namespace utils::media::encoder
