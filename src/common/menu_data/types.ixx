module;

export module Common.MenuData.Types;

import std;

export namespace Common::MenuData::Types {

// 比例预设结构体（从 Types.Presets 复制）
struct RatioPreset {
  std::wstring name;  // 比例名称
  double ratio;       // 宽高比值

  constexpr RatioPreset(const std::wstring& n, double r) noexcept : name(n), ratio(r) {}
};

// 分辨率预设结构体（从 Types.Presets 复制）
struct ResolutionPreset {
  std::wstring name;          // 显示名称（如 "4K"）
  std::uint64_t totalPixels;  // 总像素数
  int baseWidth;              // 基准宽度
  int baseHeight;             // 基准高度

  constexpr ResolutionPreset(const std::wstring& n, int w, int h) noexcept
      : name(n), totalPixels(static_cast<std::uint64_t>(w) * h), baseWidth(w), baseHeight(h) {}
};

// 计算后的功能项（新增）
struct ComputedFeatureItem {
  std::wstring text;      // 显示文本
  std::string action_id;  // 操作标识（对应原来的FeatureItem.id）
  bool enabled;           // 是否启用（来自配置）
  int order;              // 排序

  ComputedFeatureItem(const std::wstring& t, const std::string& id, bool en = true, int ord = 0)
      : text(t), action_id(id), enabled(en), order(ord) {}
};

}  // namespace Common::MenuData::Types