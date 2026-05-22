module;

export module Features.Settings.Menu.Types;

import std;

namespace Features::Settings::Menu {

export struct RatioPreset {
  std::wstring name;
  double ratio;

  constexpr RatioPreset(const std::wstring& n, double r) noexcept : name(n), ratio(r) {}
};

export struct ResolutionPreset {
  std::wstring name;
  int base_width;
  int base_height;

  constexpr ResolutionPreset(const std::wstring& n, int w, int h) noexcept
      : name(n), base_width(w), base_height(h) {}
};

}  // namespace Features::Settings::Menu
