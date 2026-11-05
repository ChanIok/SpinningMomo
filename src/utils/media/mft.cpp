#include "utils/media/mft.hpp"

#include "vendor/std.hpp"
#include "vendor/windows/codecapi.hpp"
#include "vendor/windows/combaseapi.hpp"
#include "vendor/windows/mferror.hpp"
#include "vendor/windows/strmif.hpp"

#include "utils/logger/logger.hpp"
#include "utils/string/string.hpp"

namespace utils::media::mft {

// 激活前读取身份，确保驱动 MFT 创建失败时仍能定位具体实现。
auto activation_identity(IMFActivate* activation) -> std::string {
  GUID clsid{};
  wchar_t clsid_text[39]{};
  const auto clsid_hr = activation->GetGUID(MFT_TRANSFORM_CLSID_Attribute, &clsid);
  const auto has_clsid = SUCCEEDED(clsid_hr) && StringFromGUID2(clsid, clsid_text, 39) > 0;

  wchar_t* vendor = nullptr;
  UINT32 vendor_length = 0;
  const auto vendor_hr = activation->GetAllocatedString(MFT_ENUM_HARDWARE_VENDOR_ID_Attribute,
                                                        &vendor, &vendor_length);
  auto cleanup = wil::scope_exit([&] { CoTaskMemFree(vendor); });

  return std::format("name=\"{}\", CLSID={}, vendor={}", describe(activation),
                     has_clsid ? utils::string::ToUtf8(clsid_text) : "unavailable",
                     SUCCEEDED(vendor_hr) ? utils::string::ToUtf8(vendor) : "unavailable");
}

#define MF_TRY(call)                                               \
  do {                                                             \
    const auto hr_ = (call);                                       \
    if (FAILED(hr_)) return std::unexpected(hr_error(#call, hr_)); \
  } while (false)

// 格式化 HRESULT 错误信息
auto hr_error(std::string_view action, HRESULT hr) -> std::string {
  return std::format("{}: HRESULT=0x{:08X}", action, static_cast<unsigned>(hr));
}

// 从 MFT 属性集中读取友好名称描述字符串
auto describe(IMFAttributes* attributes) -> std::string {
  wchar_t name[512]{};
  const auto hr = attributes->GetString(MFT_FRIENDLY_NAME_Attribute, name, 512, nullptr);
  return SUCCEEDED(hr) ? utils::string::ToUtf8(name) : hr_error("MFT name unavailable", hr);
}

// 根据输入输出类型与分类枚举匹配的 MFT 激活对象
auto enumerate(const GUID& category, UINT32 flags, const MFT_REGISTER_TYPE_INFO& input,
               const MFT_REGISTER_TYPE_INFO& output, IMFAttributes* attributes)
    -> Result<wil::com_ptr<IMFActivate>> {
  IMFActivate** list = nullptr;
  UINT32 count = 0;

  // 调用 MFTEnum2 进行分类枚举匹配
  const auto hr = MFTEnum2(category, flags, &input, &output, attributes, &list, &count);
  auto cleanup = wil::scope_exit([&] {
    for (UINT32 i = 0; i < count; ++i) {
      list[i]->Release();
    }
    CoTaskMemFree(list);
  });

  if (FAILED(hr)) {
    return std::unexpected(hr_error("MFTEnum2", hr));
  }

  // 每次视频编码器选择仅输出一条摘要；未选中的候选只在 Debug 下展开。
  if (category == MFT_CATEGORY_VIDEO_ENCODER) {
    Logger().info("Video encoder MFT: hardware={}, candidates={}, selected=[{}]",
                  (flags & MFT_ENUM_FLAG_HARDWARE) != 0, count,
                  count ? activation_identity(list[0]) : "none");
    for (UINT32 i = 1; i < count; ++i) {
      Logger().debug("Video encoder MFT alternative: index={}, {}", i,
                     activation_identity(list[i]));
    }
  }

  if (!count) {
    return std::unexpected("No MFT matches the selected adapter and formats");
  }

  // 选用首个匹配度最高的激活对象
  wil::com_ptr<IMFActivate> result = list[0];
  return result;
}

// 从同步或异步 MFT 中安全拉取单份输出样本，处理流格式动态协商
auto pull(IMFTransform* transform, IMFMediaType* configured)
    -> Result<std::optional<wil::com_ptr<IMFSample>>> {
  // 允许最多尝试两次，应对初次输出时的动态格式协商请求
  for (UINT attempt = 0; attempt < 2; ++attempt) {
    MFT_OUTPUT_STREAM_INFO info{};
    MF_TRY(transform->GetOutputStreamInfo(0, &info));

    wil::com_ptr<IMFSample> sample;

    // 若 MFT 不自行提供样本输出缓冲区，则需调用方主动分配内存样本
    if (!(info.dwFlags &
          (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES))) {
      if (!info.cbSize) {
        return std::unexpected("MFT requires an output buffer but reports zero size");
      }

      MF_TRY(MFCreateSample(sample.put()));
      wil::com_ptr<IMFMediaBuffer> buffer;

      if (info.cbAlignment > 1) {
        MF_TRY(MFCreateAlignedMemoryBuffer(info.cbSize, info.cbAlignment - 1, buffer.put()));
      } else {
        MF_TRY(MFCreateMemoryBuffer(info.cbSize, buffer.put()));
      }
      MF_TRY(sample->AddBuffer(buffer.get()));
    }

    MFT_OUTPUT_DATA_BUFFER output{};
    output.pSample = sample.get();
    DWORD status = 0;

    // 拉取编码输出
    const auto hr = transform->ProcessOutput(0, 1, &output, &status);
    if (output.pEvents) {
      output.pEvents->Release();
    }
    if (!sample && output.pSample) {
      sample.attach(output.pSample);
    }

    // 硬件编码器可在首次输出时要求补全输出类型；保留本段已协商的尺寸与编码配置
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      wil::com_ptr<IMFMediaType> negotiated;
      MF_TRY(transform->GetOutputAvailableType(0, 0, negotiated.put()));
      MF_TRY(configured->CopyAllItems(negotiated.get()));
      MF_TRY(transform->SetOutputType(0, negotiated.get(), 0));
      continue;
    }

    // 需要更多输入数据才能产生输出
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      return std::optional<wil::com_ptr<IMFSample>>{};
    }

    if (FAILED(hr)) {
      return std::unexpected(hr_error("ProcessOutput", hr));
    }
    if (!sample) {
      return std::unexpected("MFT returned success without an output sample");
    }

    return std::optional<wil::com_ptr<IMFSample>>(std::move(sample));
  }

  return std::unexpected("MFT repeatedly requested output type changes");
}

// 从 IMFSample 提取 PTS、DTS、时长和字节数并打包为 Packet
auto packet_from(wil::com_ptr<IMFSample> sample) -> Result<Packet> {
  Packet packet;

  // 提取显示时间戳与持续时长
  MF_TRY(sample->GetSampleTime(&packet.pts));
  MF_TRY(sample->GetSampleDuration(&packet.duration));

  // 提取解码时间戳 DTS，未指定时沿用 PTS
  UINT64 dts = 0;
  const auto hr = sample->GetUINT64(MFSampleExtension_DecodeTimestamp, &dts);
  if (hr == MF_E_ATTRIBUTENOTFOUND) {
    packet.dts = packet.pts;
  } else {
    if (FAILED(hr)) {
      return std::unexpected(hr_error("Get encoded DTS", hr));
    }
    packet.dts = std::bit_cast<LONGLONG>(dts);
  }

  // 获取有效压缩数据总字节数
  MF_TRY(sample->GetTotalLength(&packet.bytes));
  if (!packet.bytes || packet.duration <= 0) {
    return std::unexpected("Encoded sample has empty data or non-positive duration");
  }

  packet.sample = std::move(sample);
  return packet;
}

#undef MF_TRY

}  // namespace utils::media::mft
