#pragma once

#include "vendor/std.hpp"
#include "vendor/wil.hpp"
#include "vendor/windows/mfapi.hpp"
#include "vendor/windows/mfidl.hpp"
#include "vendor/windows/mftransform.hpp"

namespace utils::media::mft {

template <typename T = void>
using Result = std::expected<T, std::string>;

// 统一封装后的压缩数据包
struct Packet {
  wil::com_ptr<IMFSample> sample;
  LONGLONG pts = 0;
  LONGLONG dts = 0;
  LONGLONG duration = 0;
  DWORD bytes = 0;
};

// 格式化 HRESULT 错误信息
auto hr_error(std::string_view action, HRESULT hr) -> std::string;

// 从 MFT 属性集中读取友好名称描述字符串
auto describe(IMFAttributes* attributes) -> std::string;

// 读取激活对象的友好名称、CLSID 与厂商，用于定位实际选中的编码器实现
auto activation_identity(IMFActivate* activation) -> std::string;

// 根据输入输出类型与分类枚举全部匹配的 MFT 激活对象（按匹配度排序）
auto enumerate(const GUID& category, UINT32 flags, const MFT_REGISTER_TYPE_INFO& input,
               const MFT_REGISTER_TYPE_INFO& output, IMFAttributes* attributes = nullptr)
    -> Result<std::vector<wil::com_ptr<IMFActivate>>>;

// 从同步或异步 MFT 中安全拉取单份输出样本，处理流格式动态协商
auto pull(IMFTransform* transform, IMFMediaType* configured)
    -> Result<std::optional<wil::com_ptr<IMFSample>>>;

// 从 IMFSample 提取 PTS、DTS、时长和字节数并打包为 Packet
auto packet_from(wil::com_ptr<IMFSample> sample) -> Result<Packet>;

}  // namespace utils::media::mft
