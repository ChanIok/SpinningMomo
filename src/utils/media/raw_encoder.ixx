module;

#include <mfidl.h>
#include <mfobjects.h>

export module Utils.Media.RawEncoder;

import std;
import <audioclient.h>;
import <d3d11.h>;
import <mfapi.h>;
import <wil/com.h>;

export namespace Utils::Media::RawEncoder {

// 编码后的压缩帧
struct EncodedFrame {
  std::vector<std::uint8_t> data;  // 压缩数据
  std::int64_t timestamp_100ns;    // 时间戳（100ns 单位）
  std::int64_t duration_100ns;     // 持续时长（100ns 单位）
  bool is_keyframe;                // 是否为关键帧（仅视频）
  bool is_audio;                   // 是否为音频帧
};

// 编码器配置
struct RawEncoderConfig {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint32_t fps = 30;
  std::uint32_t bitrate = 20'000'000;
  std::uint32_t keyframe_interval = 1;  // 关键帧间隔（秒）
  bool use_hardware = true;             // 是否使用硬件编码
};

// 编码器上下文
struct RawEncoderContext {
  // 视频编码器 Transform
  wil::com_ptr<IMFTransform> video_encoder;
  wil::com_ptr<IMFDXGIDeviceManager> dxgi_manager;
  UINT dxgi_reset_token = 0;

  // 输出媒体类型（包含 SPS/PPS）
  wil::com_ptr<IMFMediaType> video_output_type;

  // 音频编码器 Transform
  wil::com_ptr<IMFTransform> audio_encoder;
  wil::com_ptr<IMFMediaType> audio_output_type;
  bool has_audio = false;

  // 缓存信息
  std::uint32_t frame_width = 0;
  std::uint32_t frame_height = 0;
  std::uint32_t fps = 30;
  bool gpu_encoding = false;

  // GPU 编码用的输入纹理（BGRA 或 NV12 取决于编码器支持）
  wil::com_ptr<ID3D11Texture2D> input_texture;

  // CPU 编码用的 staging 纹理
  wil::com_ptr<ID3D11Texture2D> staging_texture;

  // 异步 MFT 支持
  wil::com_ptr<IMFMediaEventGenerator> async_events;  // 事件生成器
  bool is_async = false;                              // 是否为异步 MFT
  bool async_need_input = false;                      // 可以接受输入
  bool async_have_output = false;                     // 有输出可用
  bool draining = false;                              // 正在排空
  bool draining_done = false;                         // 排空完成

  // BGRA→NV12 颜色空间转换（硬件编码器通常只接受 NV12）
  bool needs_nv12_conversion = false;
  wil::com_ptr<ID3D11Texture2D> nv12_texture;
  wil::com_ptr<ID3D11VideoDevice> video_device;
  wil::com_ptr<ID3D11VideoContext> video_context;
  wil::com_ptr<ID3D11VideoProcessorEnumerator> vp_enum;
  wil::com_ptr<ID3D11VideoProcessor> video_processor;
};

// 创建编码器（不创建文件，只编码）
auto create_encoder(const RawEncoderConfig& config, ID3D11Device* device,
                    WAVEFORMATEX* wave_format = nullptr)
    -> std::expected<RawEncoderContext, std::string>;

// 编码视频帧，返回压缩数据（异步模式下可能返回 0 或多帧）
auto encode_video_frame(RawEncoderContext& ctx, ID3D11DeviceContext* context,
                        ID3D11Texture2D* texture, std::int64_t timestamp_100ns)
    -> std::expected<std::vector<EncodedFrame>, std::string>;

// 编码音频，返回压缩数据
auto encode_audio_frame(RawEncoderContext& ctx, const BYTE* pcm_data, std::uint32_t pcm_size,
                        std::int64_t timestamp_100ns)
    -> std::expected<std::optional<EncodedFrame>, std::string>;

// 刷新编码器（获取所有剩余输出）
auto flush_encoder(RawEncoderContext& ctx) -> std::expected<std::vector<EncodedFrame>, std::string>;

// 获取视频 codec private data（SPS/PPS，用于 mux）
auto get_video_codec_private_data(const RawEncoderContext& ctx) -> std::vector<std::uint8_t>;

// 获取视频输出媒体类型（用于创建 SinkWriter）
auto get_video_output_type(const RawEncoderContext& ctx) -> IMFMediaType*;

// 获取音频输出媒体类型（用于创建 SinkWriter）
auto get_audio_output_type(const RawEncoderContext& ctx) -> IMFMediaType*;

// 清理编码器
auto finalize(RawEncoderContext& ctx) -> void;

}  // namespace Utils::Media::RawEncoder
