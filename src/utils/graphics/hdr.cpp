module;

module Utils.Graphics.Hdr;

import std;
import Utils.Logger;
import <dxgi1_6.h>;
import <wil/com.h>;
import <windows.h>;

namespace Utils::Graphics::Hdr {

namespace Detail {

// Windows 桌面 HDR 常用该 DXGI 色彩空间枚举值表示「HDR 正在驱动该输出」。
auto is_hdr_color_space(DXGI_COLOR_SPACE_TYPE color_space) -> bool {
  return color_space == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
}

// DXGI 偶发返回无效值；回落到 1000 nit，并夹在 lib/显示堆栈常见范围内。
auto sanitize_peak_luminance(float nits) -> float {
  if (!std::isfinite(nits) || nits <= 0.0f) {
    return 1000.0f;
  }
  return std::clamp(nits, 203.0f, 10000.0f);
}

auto make_info_from_output_desc(const DXGI_OUTPUT_DESC1& desc) -> HdrMonitorInfo {
  return HdrMonitorInfo{
      .hdr_active = is_hdr_color_space(desc.ColorSpace),
      .max_luminance_nits = sanitize_peak_luminance(desc.MaxLuminance),
  };
}

}  // namespace Detail

// 通过 HWND → HMONITOR，在 DXGI 适配器/输出枚举中找到对应输出，再读 IDXGIOutput6::GetDesc1。
auto query_monitor_hdr_info(HWND target_window) -> std::expected<HdrMonitorInfo, std::string> {
  if (!target_window || !IsWindow(target_window)) {
    return std::unexpected("Target window is invalid");
  }

  HMONITOR target_monitor = MonitorFromWindow(target_window, MONITOR_DEFAULTTONEAREST);
  if (!target_monitor) {
    return std::unexpected("Failed to resolve target monitor");
  }

  // 全局 DXGI 工厂：枚举所有 GPU 及其物理输出，直至 Monitor 句柄匹配。
  wil::com_ptr<IDXGIFactory1> factory;
  HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(factory.put()));
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create DXGI factory, HRESULT: 0x{:08X}",
                                       static_cast<unsigned int>(hr)));
  }

  for (UINT adapter_index = 0;; ++adapter_index) {
    wil::com_ptr<IDXGIAdapter1> adapter;
    hr = factory->EnumAdapters1(adapter_index, adapter.put());
    if (hr == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (FAILED(hr)) {
      Logger().warn("Failed to enumerate DXGI adapter {}, HRESULT: 0x{:08X}", adapter_index,
                    static_cast<unsigned int>(hr));
      continue;
    }

    for (UINT output_index = 0;; ++output_index) {
      wil::com_ptr<IDXGIOutput> output;
      hr = adapter->EnumOutputs(output_index, output.put());
      if (hr == DXGI_ERROR_NOT_FOUND) {
        break;
      }
      if (FAILED(hr)) {
        Logger().warn("Failed to enumerate DXGI output {}:{}, HRESULT: 0x{:08X}", adapter_index,
                      output_index, static_cast<unsigned int>(hr));
        continue;
      }

      DXGI_OUTPUT_DESC output_desc{};
      hr = output->GetDesc(&output_desc);
      if (FAILED(hr) || output_desc.Monitor != target_monitor) {
        continue;
      }

      // HDR 静态元数据（峰值亮度、色彩空间）在 DXGI 1.6 的 Output6 接口里。
      wil::com_ptr<IDXGIOutput6> output6;
      hr = output->QueryInterface(IID_PPV_ARGS(output6.put()));
      if (FAILED(hr) || !output6) {
        return std::unexpected("Matched DXGI output does not expose IDXGIOutput6");
      }

      DXGI_OUTPUT_DESC1 output_desc1{};
      hr = output6->GetDesc1(&output_desc1);
      if (FAILED(hr)) {
        return std::unexpected(
            std::format("Failed to query DXGI output HDR description, HRESULT: 0x{:08X}",
                        static_cast<unsigned int>(hr)));
      }

      return Detail::make_info_from_output_desc(output_desc1);
    }
  }

  return std::unexpected("Failed to match target monitor to a DXGI output");
}

}  // namespace Utils::Graphics::Hdr
