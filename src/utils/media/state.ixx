module;

#include <mfidl.h>
#include <mfreadwrite.h>

export module Utils.Media.Encoder.State;

import std;
import <d3d11.h>;
import <wil/com.h>;

export namespace Utils::Media::Encoder::State {

// 编码器上下文
struct EncoderContext {
  wil::com_ptr<IMFSinkWriter> sink_writer;
  DWORD video_stream_index = 0;

  // 缓存的尺寸信息
  uint32_t frame_width = 0;
  uint32_t frame_height = 0;
  DWORD buffer_size = 0;  // width * height * 4

  // CPU 编码模式
  wil::com_ptr<ID3D11Texture2D> staging_texture;  // CPU 可读的暂存纹理
  wil::com_ptr<IMFSample> reusable_sample;        // 复用的 Sample
  wil::com_ptr<IMFMediaBuffer> reusable_buffer;   // 复用的 Buffer

  // GPU 编码模式
  wil::com_ptr<IMFDXGIDeviceManager> dxgi_manager;
  UINT reset_token = 0;
  wil::com_ptr<ID3D11Texture2D> shared_texture;  // 编码器专用纹理
  bool gpu_encoding = false;

  // 音频流
  DWORD audio_stream_index = 0;  // 音频流索引
  bool has_audio = false;        // 是否有音频流

  // 注：线程同步由调用方管理，因为 std::mutex 不可移动，无法放在 std::expected 返回值中
};

}  // namespace Utils::Media::Encoder::State
