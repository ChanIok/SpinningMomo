#include "features/recording/video.hpp"

#include "vendor/std.hpp"
#include "vendor/windows/codecapi.hpp"
#include "vendor/windows/d3d11_4.hpp"
#include "vendor/windows/dxgi1_2.hpp"
#include "vendor/windows/mferror.hpp"
#include "vendor/windows/strmif.hpp"
#include "vendor/windows/winrt/windows_foundation.hpp"
#include "vendor/windows/wrl.hpp"

#include "features/recording/mux.hpp"
#include "features/recording/recording.hpp"
#include "features/recording/state.hpp"
#include "features/recording/time.hpp"
#include "features/recording/usecase.hpp"
#include "utils/graphics/capture.hpp"
#include "utils/graphics/capture_region.hpp"
#include "utils/graphics/d3d.hpp"
#include "utils/logger/logger.hpp"
#include "utils/media/video_convert.hpp"
#include "utils/string/string.hpp"

namespace features::recording::video {
namespace {

using Clock = std::chrono::steady_clock;

constexpr LONGLONG ticks_per_second = 10'000'000;
constexpr int k_capture_pool_size = 2;

#define MF_TRY(call)                                                                  \
  do {                                                                                \
    const auto hr_ = (call);                                                          \
    if (FAILED(hr_)) return std::unexpected(utils::media::mft::hr_error(#call, hr_)); \
  } while (false)

// 视频线程内部信号与纹理槽位状态追踪
struct Signals {
  std::mutex mutex;
  std::condition_variable changed;
  std::deque<std::pair<MediaEventType, HRESULT>> events;
  std::uint64_t returned = 0;
  double max_hold_ms = 0;
  bool frame_pending = false;
  std::array<bool, k_video_pool_size> busy{};
  std::array<std::uint64_t, k_video_pool_size> frames{};
  std::array<Clock::time_point, k_video_pool_size> borrowed{};
  UINT used = 0;
  UINT peak = 0;
  std::string error;
  std::atomic<bool> closing{false};
};

// 归还已借出的编码输入纹理槽位并统计租借时长
auto return_slot(const std::shared_ptr<Signals>& signals, UINT slot, std::uint64_t frame) -> void {
  std::lock_guard lock(signals->mutex);

  // 校验槽位状态与帧序号，防止重复归还或过时归还
  if (!signals->busy[slot] || signals->frames[slot] != frame) {
    signals->error = "Duplicate or stale texture return";
  } else {
    signals->busy[slot] = false;
    const auto now = Clock::now();
    --signals->used;
    ++signals->returned;
    signals->max_hold_ms =
        std::max(signals->max_hold_ms,
                 std::chrono::duration<double, std::milli>(now - signals->borrowed[slot]).count());
  }

  // 唤醒主视频线程继续借槽送帧
  signals->changed.notify_one();
}

// Tracked Sample 异步释放回调：底层 MFT 完成输入纹理读取后归还槽位
class SampleReturned final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IMFAsyncCallback> {
 public:
  SampleReturned(std::shared_ptr<Signals> signals, UINT slot, std::uint64_t frame)
      : signals_(std::move(signals)), slot_(slot), frame_(frame) {}

  STDMETHODIMP GetParameters(DWORD*, DWORD*) override { return E_NOTIMPL; }

  STDMETHODIMP Invoke(IMFAsyncResult*) override {
    return_slot(signals_, slot_, frame_);
    return S_OK;
  }

 private:
  std::shared_ptr<Signals> signals_;
  UINT slot_;
  std::uint64_t frame_;
};

// 异步硬件 MFT 事件生成器回调：监听 NeedInput、HaveOutput、DrainComplete 等事件
class EncoderEvents final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IMFAsyncCallback> {
 public:
  EncoderEvents(std::shared_ptr<Signals> signals, IMFMediaEventGenerator* generator)
      : signals_(std::move(signals)), generator_(generator) {}

  STDMETHODIMP GetParameters(DWORD*, DWORD*) override { return E_NOTIMPL; }

  STDMETHODIMP Invoke(IMFAsyncResult* result) override {
    wil::com_ptr<IMFMediaEvent> item;
    HRESULT hr = generator_->EndGetEvent(result, item.put());

    if (signals_->closing.load()) {
      return S_OK;
    }

    MediaEventType type = MEError;
    HRESULT status = hr;
    if (SUCCEEDED(hr)) {
      hr = item->GetType(&type);
    }
    if (SUCCEEDED(hr)) {
      hr = item->GetStatus(&status);
    }
    if (FAILED(hr)) {
      type = MEError;
      status = hr;
    }

    // 将收到的 MFT 事件入队并通知视频线程
    {
      std::lock_guard lock(signals_->mutex);
      signals_->events.emplace_back(type, status);
    }
    signals_->changed.notify_one();

    // 重新挂起下一个事件监听
    if (!signals_->closing.load()) {
      hr = generator_->BeginGetEvent(this, nullptr);
      if (FAILED(hr) && !signals_->closing.load()) {
        std::lock_guard lock(signals_->mutex);
        signals_->events.emplace_back(MEError, hr);
        signals_->changed.notify_one();
      }
    }
    return S_OK;
  }

 private:
  std::shared_ptr<Signals> signals_;
  wil::com_ptr<IMFMediaEventGenerator> generator_;
};

// 设置 ICodecAPI 的 32 位无符号整型参数
auto set_uint(ICodecAPI* codec, const GUID& key, ULONG value, std::string_view label)
    -> std::expected<void, std::string> {
  VARIANT requested{};
  requested.vt = VT_UI4;
  requested.ulVal = value;

  const auto hr = codec->SetValue(&key, &requested);
  if (FAILED(hr)) {
    return std::unexpected(utils::media::mft::hr_error(label, hr));
  }
  return {};
}

// 构建视频媒体类型：根据色彩位深、格式（H.264/H.265/NV12/P010）与 HDR 参数配置
auto video_type(const RecordingConfig& config, bool compressed)
    -> std::expected<wil::com_ptr<IMFMediaType>, std::string> {
  wil::com_ptr<IMFMediaType> type;
  MF_TRY(MFCreateMediaType(type.put()));

  MF_TRY(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
  MF_TRY(type->SetGUID(
      MF_MT_SUBTYPE,
      compressed ? (config.codec == VideoCodec::H265 ? MFVideoFormat_HEVC : MFVideoFormat_H264)
                 : (config.enable_hdr ? MFVideoFormat_P010 : MFVideoFormat_NV12)));

  // 分辨率、帧率与像素宽高比
  MF_TRY(MFSetAttributeSize(type.get(), MF_MT_FRAME_SIZE, config.width, config.height));
  MF_TRY(MFSetAttributeRatio(type.get(), MF_MT_FRAME_RATE, config.fps, 1));
  MF_TRY(MFSetAttributeRatio(type.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1));
  MF_TRY(type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));

  // 颜色空间与转换函数：HDR 使用 BT.2020 / PQ (SMPTE 2084)，SDR 使用 BT.709
  MF_TRY(type->SetUINT32(MF_MT_VIDEO_PRIMARIES,
                         config.enable_hdr ? MFVideoPrimaries_BT2020 : MFVideoPrimaries_BT709));
  MF_TRY(type->SetUINT32(MF_MT_TRANSFER_FUNCTION,
                         config.enable_hdr ? MFVideoTransFunc_2084 : MFVideoTransFunc_709));
  MF_TRY(type->SetUINT32(MF_MT_YUV_MATRIX, config.enable_hdr ? MFVideoTransferMatrix_BT2020_10
                                                             : MFVideoTransferMatrix_BT709));
  MF_TRY(type->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_16_235));

  if (compressed) {
    // 压缩格式：配置码率与 Profile
    MF_TRY(type->SetUINT32(MF_MT_AVG_BITRATE, config.bitrate));

    const auto profile =
        config.codec == VideoCodec::H264
            ? static_cast<UINT32>(eAVEncH264VProfile_High)
            : static_cast<UINT32>(config.enable_hdr ? eAVEncH265VProfile_Main_420_10
                                                    : eAVEncH265VProfile_Main_420_8);
    MF_TRY(type->SetUINT32(MF_MT_MPEG2_PROFILE, profile));

    // HDR 模式：写入 Mastering Display 色彩元数据及峰值亮度提示
    if (config.enable_hdr) {
      const MT_CUSTOM_VIDEO_PRIMARIES primaries{0.6800f, 0.3200f, 0.2650f, 0.6900f,
                                                0.1500f, 0.0600f, 0.3127f, 0.3290f};
      const auto peak = std::clamp<UINT>(config.hdr_target_peak_nits, 203, 10000);

      MF_TRY(type->SetBlob(MF_MT_CUSTOM_VIDEO_PRIMARIES, reinterpret_cast<const BYTE*>(&primaries),
                           sizeof(primaries)));
      MF_TRY(type->SetUINT32(MF_MT_MAX_MASTERING_LUMINANCE, peak));
      MF_TRY(type->SetUINT32(MF_MT_MIN_MASTERING_LUMINANCE, 0));
      MF_TRY(type->SetUINT32(MF_MT_MAX_LUMINANCE_LEVEL, peak));
      MF_TRY(type->SetUINT32(MF_MT_MAX_FRAME_AVERAGE_LUMINANCE_LEVEL, peak / 2));
    }
  } else {
    // 未压缩输入格式：计算图像步长与总字节大小
    UINT32 bytes = 0;
    MF_TRY(MFCalculateImageSize(config.enable_hdr ? MFVideoFormat_P010 : MFVideoFormat_NV12,
                                config.width, config.height, &bytes));
    MF_TRY(type->SetUINT32(MF_MT_DEFAULT_STRIDE, config.width * (config.enable_hdr ? 2 : 1)));
    MF_TRY(type->SetUINT32(MF_MT_SAMPLE_SIZE, bytes));
    MF_TRY(type->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE));
  }

  return type;
}

// 封装已创建并配置完成的视频编码器
struct Encoder {
  wil::com_ptr<IMFTransform> transform;
  wil::com_ptr<IMFMediaType> input;
  wil::com_ptr<IMFDXGIDeviceManager> manager;
  bool gpu = false;
  std::string name;
};

// 创建并初始化硬件或软件视频编码器 MFT：枚举适配器 → 配置低延迟与码率控制 → 绑定媒体类型
auto create_encoder(const RecordingConfig& config, ID3D11Device* device, const LUID& luid, bool gpu)
    -> std::expected<Encoder, std::string> {
  // 检查 GOP 溢出
  if (config.fps > std::numeric_limits<ULONG>::max() / 2) {
    return std::unexpected("Recording FPS overflows the two-second GOP size");
  }

  Encoder result;
  result.gpu = gpu;

  // 指定输入与输出注册类型信息
  const MFT_REGISTER_TYPE_INFO in{MFMediaType_Video,
                                  config.enable_hdr ? MFVideoFormat_P010 : MFVideoFormat_NV12};
  const MFT_REGISTER_TYPE_INFO out{MFMediaType_Video, config.codec == VideoCodec::H265
                                                          ? MFVideoFormat_HEVC
                                                          : MFVideoFormat_H264};

  // 硬件 MFT 指定当前正在运行的 GPU 适配器 LUID 进行过滤
  wil::com_ptr<IMFAttributes> filter;
  if (gpu) {
    MF_TRY(MFCreateAttributes(filter.put(), 1));
    MF_TRY(
        filter->SetBlob(MFT_ENUM_ADAPTER_LUID, reinterpret_cast<const BYTE*>(&luid), sizeof(luid)));
  }

  auto activations = utils::media::mft::enumerate(
      MFT_CATEGORY_VIDEO_ENCODER,
      (gpu ? MFT_ENUM_FLAG_HARDWARE : MFT_ENUM_FLAG_SYNCMFT) | MFT_ENUM_FLAG_SORTANDFILTER, in, out,
      filter.get());
  if (!activations) {
    return std::unexpected(activations.error());
  }

  // 视频编码器沿用匹配度最高的首个候选
  const auto& activation = activations->front();
  result.name = utils::media::mft::describe(activation.get());
  const auto activation_hr = activation->ActivateObject(IID_PPV_ARGS(result.transform.put()));
  if (FAILED(activation_hr)) {
    return std::unexpected(utils::media::mft::hr_error(
        std::format("ActivateObject video encoder \"{}\" (GPU={}, codec={}, input={}, "
                    "LUID={:08X}:{:08X})",
                    result.name, gpu, video_codec_to_string(config.codec),
                    config.enable_hdr ? "P010" : "NV12", UINT(luid.HighPart), luid.LowPart),
        activation_hr));
  }

  auto failed = wil::scope_exit([&] {
    if (result.transform) {
      MFShutdownObject(result.transform.get());
    }
  });

  auto* transform = result.transform.get();
  wil::com_ptr<IMFAttributes> attributes;
  MF_TRY(transform->GetAttributes(attributes.put()));

  // 硬件编码器要求必须支持异步处理并开启 D3D11 感知
  if (gpu) {
    UINT32 async = 0;
    UINT32 aware = 0;
    MF_TRY(attributes->GetUINT32(MF_TRANSFORM_ASYNC, &async));
    MF_TRY(attributes->GetUINT32(MF_SA_D3D11_AWARE, &aware));

    if (!async || !aware) {
      return std::unexpected(std::format(
          "Hardware video MFT must be asynchronous and D3D11-aware: async={}, D3D11_aware={}",
          async, aware));
    }

    // 解锁异步 MFT 处理模式
    MF_TRY(attributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE));

    // 创建 DXGI 设备管理器并绑定当前 D3D11 设备
    UINT token = 0;
    MF_TRY(MFCreateDXGIDeviceManager(&token, result.manager.put()));
    MF_TRY(result.manager->ResetDevice(device, token));
    MF_TRY(transform->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                                     reinterpret_cast<ULONG_PTR>(result.manager.get())));
  }

  // 通过 ICodecAPI 精确配置编码参数（速率控制、GOP、码率/质量、零 B 帧）
  wil::com_ptr<ICodecAPI> codec;
  MF_TRY(transform->QueryInterface(IID_PPV_ARGS(codec.put())));

  auto configured =
      set_uint(codec.get(), CODECAPI_AVEncCommonRateControlMode,
               config.rate_control == RateControlMode::CBR ? eAVEncCommonRateControlMode_CBR
                                                           : eAVEncCommonRateControlMode_Quality,
               "Set rate control");
  if (!configured) {
    return std::unexpected(configured.error());
  }

  // GOP 设置为两秒
  configured = set_uint(codec.get(), CODECAPI_AVEncMPVGOPSize, config.fps * 2, "Set GOP");
  if (!configured) {
    return std::unexpected(configured.error());
  }

  // 根据选定模式配置 CBR 码率、VBR 质量或 CQP QP 值
  if (config.rate_control == RateControlMode::CBR) {
    configured =
        set_uint(codec.get(), CODECAPI_AVEncCommonMeanBitRate, config.bitrate, "Set bitrate");
  } else if (config.rate_control == RateControlMode::VBR) {
    configured =
        set_uint(codec.get(), CODECAPI_AVEncCommonQuality, config.quality, "Set quality VBR");
  } else {
    VARIANT qp{};
    qp.vt = VT_UI8;
    qp.ullVal = config.qp;
    MF_TRY(codec->SetValue(&CODECAPI_AVEncVideoEncodeQP, &qp));
  }
  if (!configured) {
    return std::unexpected(configured.error());
  }

  // 禁用 B 帧（设置为 0），避免帧重排序延迟
  const auto no_b =
      set_uint(codec.get(), CODECAPI_AVEncMPVDefaultBPictureCount, 0, "Set B-frames=0");
  if (!no_b) {
    Logger().debug("Recording encoder optional B-frame control: {}", no_b.error());
  }

  // 校验并设置输入输出媒体格式
  auto input = video_type(config, false);
  auto output = video_type(config, true);
  if (!input) {
    return std::unexpected(input.error());
  }
  if (!output) {
    return std::unexpected(output.error());
  }

  MF_TRY(transform->SetOutputType(0, output->get(), 0));
  result.input = std::move(*input);
  MF_TRY(transform->SetInputType(0, result.input.get(), 0));

  failed.release();
  return result;
}

}  // namespace

// 计算捕获范围与裁剪计划：根据窗口客户区或全窗口确定偶数对齐的输出尺寸
auto resolve_capture_plan(HWND window, bool client_area, int width, int height)
    -> std::expected<CapturePlan, std::string> {
  if (width <= 0 || height <= 0) {
    return std::unexpected("Invalid capture dimensions");
  }

  CapturePlan plan;
  plan.source_width = width;
  plan.source_height = height;
  plan.region = {0, 0, UINT(width), UINT(height)};

  // 仅录制客户区时计算去除边框阴影后的裁剪区域
  if (client_area) {
    auto region =
        utils::graphics::capture_region::calculate_client_crop_region(window, width, height);
    if (!region) {
      return std::unexpected(region.error());
    }
    plan.region = *region;
  }

  // 强制向下取偶数，满足 H.264 / H.265 对齐要求
  plan.region.width &= ~1u;
  plan.region.height &= ~1u;
  if (!plan.region.width || !plan.region.height) {
    return std::unexpected("Empty capture region");
  }

  plan.output_width = plan.region.width;
  plan.output_height = plan.region.height;
  plan.should_crop = plan.region.left || plan.region.top || plan.region.width != UINT(width) ||
                     plan.region.height != UINT(height);

  return plan;
}

// 视频工作线程主循环：创建 D3D11 设备 → 协商硬件/软件 MFT → 启动 WGC 捕获 → 帧转换与编码输出 →
// 排空收尾
auto run(core::AppState& app_state) -> std::expected<void, std::string> {
  auto& session = *app_state.recording->session;
  const auto& config = session.config;
  auto signals = std::make_shared<Signals>();

  // 创建无头 Direct3D 11 设备并启用多线程安全保护
  auto d3d =
      utils::graphics::d3d::create_headless_d3d_device(session.prefer_high_performance_gpu, true);
  if (!d3d) {
    return std::unexpected(d3d.error());
  }

  auto device = d3d->first;
  auto context = d3d->second;

  wil::com_ptr<ID3D11Multithread> multithread;
  MF_TRY(device->QueryInterface(IID_PPV_ARGS(multithread.put())));
  multithread->SetMultithreadProtected(TRUE);

  // 获取 GPU 适配器描述与 LUID
  wil::com_ptr<IDXGIDevice> dxgi;
  wil::com_ptr<IDXGIAdapter> adapter;
  DXGI_ADAPTER_DESC desc{};
  MF_TRY(device->QueryInterface(IID_PPV_ARGS(dxgi.put())));
  MF_TRY(dxgi->GetAdapter(adapter.put()));
  MF_TRY(adapter->GetDesc(&desc));

  // 创建 WGC 所需的 WinRT Direct3D 设备包装
  auto winrt_device = utils::graphics::capture::create_winrt_device(device.get());
  if (!winrt_device) {
    return std::unexpected(winrt_device.error());
  }

  auto size = utils::graphics::capture::get_capture_item_size(session.target_window);
  if (!size) {
    return std::unexpected(size.error());
  }

  // 配置 WGC 捕获会话参数（光标、HDR scRGB 像素格式、高帧率最小刷新间隔）
  utils::graphics::capture::CaptureSessionOptions options;
  options.capture_cursor = config.capture_cursor;
  options.border_required = false;
  if (config.fps > 60) {
    options.min_update_interval = std::chrono::milliseconds(1);
  }
  if (config.enable_hdr) {
    options.pixel_format = winrt::Windows::Graphics::DirectX::DirectXPixelFormat::R16G16B16A16Float;
  }

  // 创建 WGC 捕获会话，帧到达时仅发送信号唤醒视频线程，不直接读取纹理
  auto captured = utils::graphics::capture::create_capture_session_with_frame_notification(
      session.target_window, *winrt_device, size->first, size->second,
      [signals] {
        std::lock_guard lock(signals->mutex);
        signals->frame_pending = true;
        signals->changed.notify_one();
      },
      k_capture_pool_size, options);
  if (!captured) {
    return std::unexpected(captured.error());
  }

  auto capture = std::move(*captured);
  int pool_width = size->first;
  int pool_height = size->second;

  auto close_capture =
      wil::scope_exit([&] { utils::graphics::capture::cleanup_capture_session(capture); });

  auto capturing = utils::graphics::capture::start_capture(capture);
  if (!capturing) {
    return std::unexpected(capturing.error());
  }

  wil::com_ptr<ID3D11Texture2D> latest;
  LONGLONG last_capture_qpc = 0;

  // 最多取一池画面，只复制最新一张；WGC 帧不进入编码输入队列。
  auto consume = [&]() -> std::expected<bool, std::string> {
    utils::graphics::capture::Direct3D11CaptureFrame newest{nullptr};

    for (int i = 0; i < k_capture_pool_size; ++i) {
      auto frame = utils::graphics::capture::try_get_next_frame(capture);
      if (!frame) {
        break;
      }
      ++session.video_captured;
      if (newest) {
        newest.Close();
      }
      newest = std::move(frame);
    }

    if (!newest) {
      return false;
    }

    auto close_frame = wil::scope_exit([&] { newest.Close(); });
    const auto content = newest.ContentSize();
    if (content.Width <= 0 || content.Height <= 0) {
      return false;
    }

    // 重新校验窗口物理尺寸
    auto plan = resolve_capture_plan(session.target_window, config.capture_client_area,
                                     content.Width, content.Height);
    if (!plan) {
      return std::unexpected(plan.error());
    }

    // 检测到录制过程中分辨率发生变化
    if (plan->output_width != config.width || plan->output_height != config.height) {
      if (session.started.load() && config.auto_restart_on_resize) {
        // 请求控制线程平滑切段重启
        request_control_action(app_state, RecordingControlAction::RestartAfterResize);
        return false;
      }
      return std::unexpected("Recording output dimensions changed");
    }

    // 内部帧池尺寸失配时重建帧池
    if (content.Width != pool_width || content.Height != pool_height) {
      newest.Close();
      close_frame.release();

      auto resized =
          utils::graphics::capture::recreate_frame_pool(capture, content.Width, content.Height);
      if (!resized) {
        return std::unexpected(resized.error());
      }
      pool_width = content.Width;
      pool_height = content.Height;
      return false;
    }

    // 获取并裁剪纹理至目标工作尺寸
    auto texture =
        utils::graphics::capture::get_dxgi_interface_from_object<ID3D11Texture2D>(newest.Surface());
    if (!texture) {
      return std::unexpected("Cannot get WGC texture");
    }

    auto copied = utils::graphics::capture_region::crop_texture_to_region(
        device.get(), context.get(), texture.get(), plan->region, latest);
    if (!copied) {
      return std::unexpected(copied.error());
    }

    last_capture_qpc = newest.SystemRelativeTime().count();
    return true;
  };

  // 等待 WGC 交付第一帧画面（超时 5 秒）
  const auto first_frame_deadline = Clock::now() + std::chrono::seconds(5);
  while (!latest && !session.failed.load()) {
    auto frame = consume();
    if (!frame) {
      return std::unexpected(frame.error());
    }
    if (latest) {
      break;
    }
    if (Clock::now() > first_frame_deadline) {
      return std::unexpected("WGC did not deliver first frame within 5s");
    }
    if (session.stop_qpc_100ns.load() >= 0) {
      return std::unexpected("Recording startup cancelled");
    }

    std::unique_lock lock(signals->mutex);
    signals->changed.wait_for(lock, std::chrono::milliseconds(10));
  }

  if (session.failed.load()) {
    return {};
  }

  // 初始化 Direct3D 颜色空间与像素格式转换器（SDR BGRA -> NV12, HDR scRGB -> P010）
  auto conversion = utils::media::video_convert::create_converter(device.get(), config.width,
                                                                  config.height, config.enable_hdr);
  if (!conversion) {
    return std::unexpected(conversion.error());
  }

  // 预先分配固定 4 槽位有界目标纹理池
  std::array<utils::media::video_convert::ConversionTarget, k_video_pool_size> textures;
  for (auto& target : textures) {
    auto created = utils::media::video_convert::create_target(*conversion);
    if (!created) {
      return std::unexpected(created.error());
    }
    target = std::move(*created);
  }

  // 创建视频编码器（Auto 模式在硬件失败时可平滑降级至 CPU 软件 H.264）
  auto created = create_encoder(config, device.get(), desc.AdapterLuid,
                                config.encoder_mode != EncoderMode::CPU);
  if (!created && config.encoder_mode == EncoderMode::Auto && config.codec == VideoCodec::H264 &&
      !config.enable_hdr) {
    Logger().warn("Recording Auto: hardware unavailable ({}), selecting software H.264",
                  created.error());
    created = create_encoder(config, device.get(), desc.AdapterLuid, false);
  }
  if (!created) {
    return std::unexpected(created.error());
  }

  auto encoder = std::move(*created);
  wil::com_ptr<IMFMediaEventGenerator> generator;
  Microsoft::WRL::ComPtr<EncoderEvents> listener;

  auto shutdown = wil::scope_exit([&] {
    signals->closing.store(true);
    if (encoder.transform) {
      MFShutdownObject(encoder.transform.get());
    }
    encoder.transform.reset();
    generator.reset();
    listener.Reset();
  });

  // 硬件异步 MFT 监听事件并跟踪输入信用额度（credits）
  UINT credits = encoder.gpu ? 0 : 1;
  if (encoder.gpu) {
    MF_TRY(encoder.transform->QueryInterface(IID_PPV_ARGS(generator.put())));
    listener = Microsoft::WRL::Make<EncoderEvents>(signals, generator.get());
    if (!listener) {
      return std::unexpected("Cannot allocate MFT event callback");
    }
    MF_TRY(generator->BeginGetEvent(listener.Get(), nullptr));
  }

  MF_TRY(encoder.transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0));
  MF_TRY(encoder.transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0));

  // 等待硬件 MFT 请求首个输入额度（超时 5 秒）
  const auto first_credit_deadline = Clock::now() + std::chrono::seconds(5);
  while (!credits && !session.failed.load()) {
    if (session.stop_qpc_100ns.load() >= 0) {
      return std::unexpected("Recording startup cancelled");
    }

    std::unique_lock lock(signals->mutex);
    signals->changed.wait_for(lock, std::chrono::milliseconds(10));

    while (!signals->events.empty()) {
      const auto [type, status] = signals->events.front();
      signals->events.pop_front();

      if (FAILED(status)) {
        return std::unexpected(utils::media::mft::hr_error("Initial video MFT event", status));
      }
      if (type == MEError) {
        return std::unexpected("Initial video MFT error");
      }
      if (type == METransformHaveOutput) {
        return std::unexpected("Video output before first input");
      }
      if (type == METransformNeedInput) {
        ++credits;
      }
    }

    if (Clock::now() > first_credit_deadline) {
      return std::unexpected("Video MFT did not request first input within 5s");
    }
  }

  Logger().info(
      "Recording video: {}, GPU={}, adapter={}, LUID={:08X}:{:08X}, {}x{}@{}, HDR={}, "
      "wgc_pool={} latest=1 input_pool={}",
      encoder.name, encoder.gpu, utils::string::ToUtf8(desc.Description),
      UINT(desc.AdapterLuid.HighPart), desc.AdapterLuid.LowPart, config.width, config.height,
      config.fps, config.enable_hdr, k_capture_pool_size, k_video_pool_size);

  // 广播视频流已准备就绪（stream 0 ready）
  {
    std::lock_guard lock(session.mutex);
    session.streams[0].ready = true;
    session.changed.notify_all();
  }

  // 等待会话正式开始
  if (!wait_session_start(app_state)) {
    return {};
  }

  std::deque<LONGLONG> expected;
  std::optional<utils::media::mft::Packet> pending;
  std::optional<LONGLONG> last_input;
  wil::com_ptr<ID3D11Texture2D> staging;

  // CPU 软件模式需要额外创建 Staging 读回纹理
  if (!encoder.gpu) {
    D3D11_TEXTURE2D_DESC staging_desc{};
    textures[0].texture->GetDesc(&staging_desc);
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    MF_TRY(device->CreateTexture2D(&staging_desc, nullptr, staging.put()));
  }

  bool type_published = false;

  // 从 MFT 拉取压缩输出包的闭包：严格校验 PTS/DTS，以保留包确定前一个包的 duration
  const auto pull_mode = encoder.gpu ? utils::media::mft::PullMode::asynchronous
                                     : utils::media::mft::PullMode::synchronous;
  auto collect = [&]() -> std::expected<void, std::string> {
    while (!session.failed.load()) {
      session.video_stage.store("ProcessOutput");
      auto sample = utils::media::mft::pull(encoder.transform.get(), pull_mode);
      if (!sample) {
        return std::unexpected(sample.error());
      }
      if (!*sample) {
        return {};
      }

      auto packet = utils::media::mft::packet_from(std::move(**sample));
      if (!packet) {
        return std::unexpected(packet.error());
      }

      // 验证输出时间戳单调递增，且禁止 B 帧重排序
      if (expected.empty() || std::llabs(packet->pts - expected.front()) > 1 ||
          packet->dts != packet->pts) {
        return std::unexpected(
            std::format("Video timestamp changed/reordered: expected={} PTS={} DTS={}",
                        expected.empty() ? -1 : expected.front(), packet->pts, packet->dts));
      }
      expected.pop_front();
      ++session.video_encoded;

      session.video_delay_100ns.store(time::elapsed_since_start_100ns(session.start_qpc_100ns) -
                                      packet->pts);

      // 首次获取到实际输出格式时发布给封装线程
      if (!type_published) {
        wil::com_ptr<IMFMediaType> negotiated;
        MF_TRY(encoder.transform->GetOutputCurrentType(0, negotiated.put()));

        std::lock_guard lock(session.mutex);
        session.streams[0].type = std::move(negotiated);
        type_published = true;
        session.changed.notify_all();
      }

      // 用后一帧的 PTS 准确计算前一帧的持续时长并投递到封装队列
      if (pending) {
        pending->duration = packet->pts - pending->pts;
        if (pending->duration <= 0) {
          return std::unexpected("Non-increasing video PTS");
        }
        MF_TRY(pending->sample->SetSampleDuration(pending->duration));

        auto queued = mux::enqueue(app_state, 0, std::move(*pending));
        if (!queued) {
          return queued;
        }
      }
      pending = std::move(*packet);
      if (pull_mode == utils::media::mft::PullMode::asynchronous) {
        return {};
      }
    }

    return {};
  };

  // 首个尚未提交、也尚未过期的时间格；暂时无额度或空槽不消耗当前格。
  std::uint64_t next = 0;
  auto expire_before = [&](std::uint64_t end) {
    if (end > next) {
      session.video_skipped.fetch_add(end - next);
      next = end;
    }
  };
  bool draining = false;
  bool drained = false;
  bool notified = false;
  bool write_backlog_notified = false;
  auto last_progress = Clock::now();
  auto drain_started = Clock::now();
  auto next_log = Clock::now() + std::chrono::seconds(1);
  std::uint64_t last_skipped = 0;

  // 视频主处理循环：驱动帧节拍、转换输入、处理异步事件并收集输出
  while (!drained && !session.failed.load()) {
    std::deque<std::pair<MediaEventType, HRESULT>> events;
    bool new_frame;
    UINT pool_used;
    std::uint64_t returned;

    // 提取 MFT 事件与捕获更新信号
    {
      std::lock_guard lock(signals->mutex);
      if (!signals->error.empty()) {
        return std::unexpected(signals->error);
      }
      events.swap(signals->events);
      new_frame = std::exchange(signals->frame_pending, false);
      session.pool_peak.store(signals->peak);
      pool_used = signals->used;
      returned = signals->returned;
    }

    // 消费所有 MFT 事件
    for (const auto& [type, status] : events) {
      if (FAILED(status)) {
        return std::unexpected(utils::media::mft::hr_error("Video MFT event", status));
      }
      if (type == MEError) {
        return std::unexpected("Video MFT emitted MEError");
      }
      if (type == METransformNeedInput && !draining) {
        ++credits;
      }
      if (type == METransformHaveOutput) {
        auto output = collect();
        if (!output) {
          return output;
        }
        last_progress = Clock::now();
      }
      if (type == METransformDrainComplete) {
        drained = true;
      }
    }

    const auto stop = session.stop_qpc_100ns.load();

    // 触发停止：关闭 WGC 捕获并向 MFT 发送 Drain 指令
    if (stop >= 0 && !draining) {
      // 按请求时冻结的截止点结清未提交时间格，不把设备关闭和排空耗时算成欠帧。
      const auto duration = time::relative_timestamp_100ns(session.start_qpc_100ns, stop);
      auto end = static_cast<std::uint64_t>(time::sample_index_at(duration, config.fps));
      if (time::timestamp_for_index(end, config.fps) < duration) {
        ++end;
      }
      expire_before(end);
      draining = true;
      drain_started = Clock::now();
      utils::graphics::capture::stop_capture_session(capture);

      MF_TRY(encoder.transform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0));
      MF_TRY(encoder.transform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0));

      if (!encoder.gpu) {
        auto output = collect();
        if (!output) {
          return output;
        }
        drained = true;
      }
    }

    // 更新 latest 不依赖编码额度；WGC 暂无新帧时继续保留原画面。
    if (!draining && new_frame) {
      session.video_stage.store("capture_latest");
      auto consumed = consume();
      if (!consumed) {
        return std::unexpected(consumed.error());
      }
    }

    // 根据 QPC 时钟计算当前理论应该推进到的时间格 target
    const auto elapsed = time::elapsed_since_start_100ns(session.start_qpc_100ns);
    const auto target = std::uint64_t(elapsed) * config.fps / ticks_per_second;

    // 按录制时钟送 latest，不要求 WGC 在这一格交付新画面。
    if (!draining && session.stop_qpc_100ns.load() < 0 && target >= next) {
      // 只放弃已经过去的时间格；当前格仍可等待额度和纹理归还。
      expire_before(target);

      std::optional<UINT> slot;

      // 拥有 MFT 输入额度且在池子安全上限内时，租借一个空闲纹理槽位
      if (credits && expected.size() < k_video_pool_size) {
        std::lock_guard lock(signals->mutex);
        for (UINT i = 0; i < k_video_pool_size; ++i) {
          if (signals->busy[i]) {
            continue;
          }
          slot = i;
          signals->busy[i] = true;
          signals->frames[i] = target;
          signals->borrowed[i] = Clock::now();
          signals->peak = std::max(signals->peak, ++signals->used);
          break;
        }
      }

      if (slot) {
        bool armed = false;
        auto unsubmitted = wil::scope_exit([&] {
          if (!armed) {
            return_slot(signals, *slot, target);
          }
        });

        // 调用 Direct3D Compute Shader 将当前画面转换为 NV12 / P010
        session.video_stage.store("convert");
        auto converted = utils::media::video_convert::convert_frame(*conversion, context.get(),
                                                                    latest.get(), textures[*slot]);
        if (!converted) {
          return std::unexpected(converted.error());
        }
        context->Flush();

        // 构造 Tracked Sample，将样本生命周期与槽位归还关联
        wil::com_ptr<IMFTrackedSample> tracked;
        wil::com_ptr<IMFSample> sample;
        wil::com_ptr<IMFMediaBuffer> buffer;
        MF_TRY(MFCreateTrackedSample(tracked.put()));
        MF_TRY(tracked->QueryInterface(IID_PPV_ARGS(sample.put())));

        UINT32 bytes = 0;
        MF_TRY(encoder.input->GetUINT32(MF_MT_SAMPLE_SIZE, &bytes));

        if (encoder.gpu) {
          // 硬件编码：直接绑定 DXGI Surface 缓冲区，零内存拷贝
          MF_TRY(MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), textures[*slot].texture.get(),
                                           0, FALSE, buffer.put()));
        } else {
          // 软件编码：通过 Staging 纹理将显存 NV12 读回系统内存
          session.video_stage.store("readback");
          context->CopyResource(staging.get(), textures[*slot].texture.get());

          D3D11_MAPPED_SUBRESOURCE mapped{};
          MF_TRY(context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped));
          auto unmap = wil::scope_exit([&] { context->Unmap(staging.get(), 0); });

          MF_TRY(MFCreateMemoryBuffer(bytes, buffer.put()));
          BYTE* destination = nullptr;
          MF_TRY(buffer->Lock(&destination, nullptr, nullptr));

          for (UINT y = 0; y < config.height + config.height / 2; ++y) {
            std::memcpy(destination + std::size_t(y) * config.width,
                        static_cast<const BYTE*>(mapped.pData) + std::size_t(y) * mapped.RowPitch,
                        config.width);
          }
          MF_TRY(buffer->Unlock());
        }

        MF_TRY(buffer->SetCurrentLength(bytes));
        MF_TRY(sample->AddBuffer(buffer.get()));

        // 计算该帧时间戳与单帧理论时长
        const auto pts = time::timestamp_for_index(target, config.fps);
        MF_TRY(sample->SetSampleTime(pts));
        MF_TRY(sample->SetSampleDuration(time::timestamp_for_index(target + 1, config.fps) - pts));

        // 存在跳帧间隙时标记 Discontinuity
        if (last_input && pts - *last_input > (ticks_per_second + config.fps - 1) / config.fps) {
          MF_TRY(sample->SetUINT32(MFSampleExtension_Discontinuity, TRUE));
        }

        // 注册样本归还回调，在编码器用完后释放槽位
        auto callback = Microsoft::WRL::Make<SampleReturned>(signals, *slot, target);
        if (!callback) {
          return std::unexpected("Cannot allocate texture return callback");
        }
        MF_TRY(tracked->SetAllocator(callback.Get(), nullptr));

        armed = true;
        tracked.reset();

        // 转换期间收到停止请求时，释放这份未提交样本，下一轮按截止点排空。
        if (session.stop_qpc_100ns.load() >= 0) {
          continue;
        }

        // 提交帧至视频 MFT 输入
        session.video_stage.store("ProcessInput");
        MF_TRY(encoder.transform->ProcessInput(0, sample.get(), 0));

        if (encoder.gpu) {
          --credits;
        }

        expected.push_back(pts);
        last_input = pts;
        next = target + 1;
        ++session.video_submitted;

        sample.reset();
        buffer.reset();

        // 同步软件 MFT 立即轮询拉取输出
        if (!encoder.gpu) {
          auto output = collect();
          if (!output) {
            return output;
          }
          if (expected.empty()) {
            last_progress = Clock::now();
          }
        }
      }
      // 暂时不能提交就保持 next，收到通知后重新选择当前最新格，不补历史帧。
    }

    // 监控编码器死锁：已送入帧但在 10 秒内未产生任何输出判定为死锁
    if (!expected.empty() && Clock::now() - last_progress > std::chrono::seconds(10)) {
      return std::unexpected("Video encoder produced no output for 10 seconds with accepted input");
    }

    // 监控 Drain 超时：收尾排空耗时超过 30 秒判定为超时
    if (draining && !drained && Clock::now() - drain_started > std::chrono::seconds(30)) {
      return std::unexpected("Video encoder drain exceeded 30 seconds");
    }

    // 定期（每秒）状态自检、过载告警与统计日志打印
    if (Clock::now() >= next_log) {
      // 检查捕获的目标窗口是否已被关闭
      if (!IsWindow(session.target_window)) {
        return std::unexpected("Recording target window closed");
      }

      const auto skipped = session.video_skipped.load();
      if (!notified && skipped - last_skipped >= 6) {
        notified = true;
        notify_recording_overload(app_state);
      }
      last_skipped = skipped;

      std::size_t queued;
      std::size_t pcm_frames;
      std::int64_t packet_warning_since;
      bool audio_available;
      {
        std::lock_guard lock(session.mutex);
        queued = session.packet_bytes;
        pcm_frames = session.pcm_frames;
        packet_warning_since = session.packet_warning_since_100ns;
        audio_available = session.audio_available;
      }

      // 复用状态巡检；写盘线程即使阻塞，也不影响告警判断。
      if (!write_backlog_notified && packet_warning_since &&
          time::query_qpc_100ns() - packet_warning_since >= ticks_per_second) {
        write_backlog_notified = true;
        Logger().warn("Recording file output backlog: queued_bytes={}", queued);
        notify_recording_write_backlog(app_state);
      }

      Logger().debug(
          "Recording progress: captured={} video={}/{} skipped={} audio={} pool={}/{} "
          "compressed_bytes={} pcm_frames={} audio_available={} capture_age_ms={:.3f} "
          "encode_delay_ms={:.3f} stages={}/{}/{}",
          session.video_captured.load(), session.video_encoded.load(),
          session.video_submitted.load(), session.video_skipped.load(),
          session.audio_encoded.load(), pool_used, session.pool_peak.load(), queued, pcm_frames,
          audio_available, (time::query_qpc_100ns() - last_capture_qpc) / 10000.0,
          session.video_delay_100ns.load() / 10000.0, session.video_stage.load(),
          session.audio_stage.load(), session.mux_stage.load());

      next_log = Clock::now() + std::chrono::seconds(1);
    }

    // 等待 MFT、WGC、纹理归还或 1ms 超时；已有通知会立即进入下一轮处理。
    session.video_stage.store(draining ? "drain_wait" : "wait_event_or_tick");
    std::unique_lock lock(signals->mutex);
    signals->changed.wait_for(lock, std::chrono::milliseconds(1), [&] {
      return !signals->events.empty() || signals->frame_pending || signals->returned != returned ||
             !signals->error.empty();
    });
  }

  if (session.failed.load()) {
    return {};
  }

  // 确保所有送入的帧都已被排空并产生输出
  if (!expected.empty()) {
    return std::unexpected("Video drain lost accepted input");
  }

  // 若起录后立即停止且未产生任何送帧，交给短段丢弃逻辑处理
  if (!pending) {
    return {};
  }

  // 将末帧的持续时间延伸至录制截止时间点，防止播放结尾闪烁
  pending->duration = session.stop_qpc_100ns.load() - session.start_qpc_100ns - pending->pts;
  if (pending->duration <= 0) {
    return std::unexpected("Video extends beyond recording cutoff");
  }

  MF_TRY(pending->sample->SetSampleDuration(pending->duration));
  auto queued = mux::enqueue(app_state, 0, std::move(*pending));
  if (!queued) {
    return queued;
  }

  // 彻底关闭并注销视频 MFT
  signals->closing.store(true);
  if (encoder.gpu) {
    MF_TRY(MFShutdownObject(encoder.transform.get()));
  }

  encoder.transform.reset();
  generator.reset();
  listener.Reset();

  // 等待底层硬件驱动归还全部租借的纹理槽位（超时 5 秒）
  {
    std::unique_lock lock(signals->mutex);
    if (!signals->changed.wait_for(lock, std::chrono::seconds(5), [&] { return !signals->used; })) {
      return std::unexpected("Video input leases did not return after encoder shutdown");
    }
    if (!signals->error.empty()) {
      return std::unexpected(signals->error);
    }

    Logger().info("Recording video drain: {:.3f}ms; pool peak={} returns={} max_hold={:.3f}ms",
                  std::chrono::duration<double, std::milli>(Clock::now() - drain_started).count(),
                  signals->peak, signals->returned, signals->max_hold_ms);
  }

  session.video_stage.store("done");
  return {};
}

#undef MF_TRY

}  // namespace features::recording::video
