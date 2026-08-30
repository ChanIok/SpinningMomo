#include "features/adb_mode/display_control.hpp"

#include "vendor/std.hpp"

#include "features/adb_mode/adb_client.hpp"
#include "features/window_control/window_control.hpp"

namespace features::adb_mode::display_control {

namespace {

// 从 adb wm size 输出中读取指定标签后的宽高。
auto parse_resolution(std::string_view output, std::string_view label)
    -> std::optional<Resolution> {
  const auto pattern = std::format(R"({}\s*(\d+)\s*x\s*(\d+))", label);
  const std::regex expression(pattern, std::regex_constants::icase);
  std::cmatch match;
  const std::string output_copy(output);
  if (!std::regex_search(output_copy.c_str(), match, expression) || match.size() < 3) {
    return std::nullopt;
  }

  try {
    const int width = std::stoi(match[1].str());
    const int height = std::stoi(match[2].str());
    if (width <= 0 || height <= 0) {
      return std::nullopt;
    }
    return Resolution{.width = width, .height = height};
  } catch (...) {
    // 极端长数字可能让 stoi 抛出异常，解析失败按缺失尺寸处理。
    return std::nullopt;
  }
}

// 判断设置项是否代表“使用设备原始尺寸”的 Default 预设。
auto is_default_preset(const features::window_control::ResolutionPresetInput& preset) -> bool {
  return preset.base_width <= 0 && preset.base_height <= 0;
}

// 执行 wm size 命令；有尺寸时设置覆盖，没有尺寸时重置覆盖。
auto set_size(const AdbConnectionConfig& config, std::string_view serial,
              std::optional<Resolution> size_override) -> std::expected<void, std::string> {
  const std::wstring command =
      size_override ? std::format(L"{}x{}", size_override->width, size_override->height) : L"reset";
  auto result = adb::run_on_device(config, serial, {L"shell", L"wm", L"size", command});
  if (!result) {
    return std::unexpected(result.error());
  }
  return {};
}

}  // namespace

// 查询设备物理尺寸，忽略连接前可能存在的 override。
auto query(const AdbConnectionConfig& config, std::string_view serial)
    -> std::expected<Resolution, std::string> {
  auto size_result = adb::run_on_device(config, serial, {L"shell", L"wm", L"size"});
  if (!size_result) {
    return std::unexpected("Failed to query ADB device display size: " + size_result.error());
  }

  // 物理尺寸是恢复和比例计算必须存在的基准。
  auto physical_size = parse_resolution(size_result->stdout_data, "Physical size:");
  if (!physical_size) {
    return std::unexpected("ADB device display query did not contain a physical size");
  }

  return *physical_size;
}

// 校验目标尺寸后设置设备的显示覆盖尺寸。
auto set(const AdbConnectionConfig& config, std::string_view serial, const Resolution& size)
    -> std::expected<void, std::string> {
  if (size.width <= 0 || size.height <= 0) {
    return std::unexpected("ADB device display size must be positive");
  }

  auto size_result = set_size(config, serial, size);
  if (!size_result) {
    return std::unexpected(size_result.error());
  }
  return {};
}

// 清除显示覆盖，让设备回到系统默认显示尺寸。
auto reset(const AdbConnectionConfig& config, std::string_view serial)
    -> std::expected<void, std::string> {
  auto size_result = set_size(config, serial, std::nullopt);
  if (!size_result) {
    return std::unexpected("Failed to reset ADB device display size: " + size_result.error());
  }
  return {};
}

// 根据宽高比和预设，以设备物理尺寸为基准计算目标分辨率。
auto calculate_target_resolution(double ratio,
                                 const features::window_control::ResolutionPresetInput& preset,
                                 const Resolution& base, bool align_to_8, bool use_short_edge)
    -> Resolution {
  if (is_default_preset(preset)) {
    return base;
  }

  const auto calculated = features::window_control::calculate_resolution_from_preset(
      ratio, preset,
      features::window_control::ResolutionCalculationOptions{
          .align_to_8 = align_to_8,
          .use_short_edge = use_short_edge,
          .screen_width = base.width,
          .screen_height = base.height,
      });
  return Resolution{.width = calculated.width, .height = calculated.height};
}

}  // namespace features::adb_mode::display_control
