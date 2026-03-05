module;

export module Features.Gallery.Color.Types;

import std;

namespace Features::Gallery::Color::Types {

export struct LabColor {
  float l = 0.0f;
  float a = 0.0f;
  float b = 0.0f;
  int l_bin = 0;
  int a_bin = 0;
  int b_bin = 0;
};

export struct ExtractedColor {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  float lab_l = 0.0f;
  float lab_a = 0.0f;
  float lab_b = 0.0f;
  float weight = 0.0f;  // 0-1
  int l_bin = 0;
  int a_bin = 0;
  int b_bin = 0;
};

export struct MainColorExtractOptions {
  std::uint32_t sample_short_edge = 128;
  std::uint32_t max_samples = 8000;
  std::uint32_t cluster_count = 8;
  std::uint32_t min_colors = 3;
  std::uint32_t max_colors = 8;
  float min_weight = 0.03f;
  float coverage = 0.85f;
  float merge_delta_e = 8.0f;
  float l_bin_size = 5.0f;
  float ab_bin_size = 8.0f;
};

}  // namespace Features::Gallery::Color::Types
