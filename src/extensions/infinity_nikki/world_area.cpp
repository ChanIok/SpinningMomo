module;

module Extensions.InfinityNikki.WorldArea;

import std;

namespace Extensions::InfinityNikki::WorldArea {

constexpr std::string_view kDefaultWorldId = "1";

const std::vector<WorldPolygonRule> kWorldPolygonRules = {
    {
        .world_id = "14000000",
        .polygon =
            {
                {.x = -46005.593, .y = 77661.340},
                {.x = -46005.593, .y = 47661.340},
                {.x = -41005.593, .y = 27661.340},
                {.x = -31005.593, .y = 17661.340},
                {.x = -18005.593, .y = 7661.340},
                {.x = 23994.407, .y = 7661.340},
                {.x = 53994.407, .y = 31661.340},
                {.x = 63994.407, .y = 57661.340},
                {.x = 53994.407, .y = 77661.340},
                {.x = 23994.407, .y = 83661.340},
            },
        .z_range = ZRange{.min = -20000.0, .max = -16000.0},
    },
    {
        .world_id = "10000001",
        .polygon =
            {
                {.x = -256683.761, .y = 545333.456},
                {.x = -231683.761, .y = 539333.456},
                {.x = -206683.761, .y = 545333.456},
                {.x = -181683.761, .y = 575333.456},
                {.x = -196683.761, .y = 600333.456},
                {.x = -236683.761, .y = 610333.456},
                {.x = -266683.761, .y = 605333.456},
                {.x = -276683.761, .y = 575333.456},
            },
        .z_range = ZRange{.min = 0.0, .max = 55000.0},
    },
    {
        .world_id = "10000002",
        .polygon =
            {
                {.x = -56945.06811, .y = 8981.391768},
                {.x = -14945.06811, .y = -3018.608232},
                {.x = 7054.93189, .y = 8981.391768},
                {.x = 20054.93189, .y = 40981.391768},
                {.x = -14945.06811, .y = 72981.391768},
                {.x = -56945.06811, .y = 72981.391768},
                {.x = -62945.06811, .y = 36981.391768},
            },
        .z_range = ZRange{.min = 0.0, .max = 32000.0},
    },
    {
        .world_id = "10000010",
        .polygon =
            {
                {.x = -324475.09316, .y = 186121.062976},
                {.x = -282475.09316, .y = 184121.062976},
                {.x = -212475.09316, .y = 264121.062976},
                {.x = -232475.09316, .y = 324121.062976},
                {.x = -262475.09316, .y = 334121.062976},
                {.x = -322475.09316, .y = 314121.062976},
                {.x = -332475.09316, .y = 214121.062976},
            },
        .z_range = ZRange{.min = -20000.0, .max = 5000.0},
    },
    {
        .world_id = "10000027",
        .polygon =
            {
                {.x = -360200.62985, .y = 149027.655483},
                {.x = -326200.62985, .y = 150027.655483},
                {.x = -242200.62985, .y = 225027.655483},
                {.x = -216200.62985, .y = 270027.655483},
                {.x = -250200.62985, .y = 310027.655483},
                {.x = -312200.62985, .y = 305027.655483},
                {.x = -372200.62985, .y = 205027.655483},
            },
        .z_range = ZRange{.min = -20000.0, .max = 5000.0},
    },
    {
        .world_id = "4020034",
        .polygon =
            {
                {.x = 42247.93604, .y = -300.600838},
                {.x = 82247.93604, .y = -300.600838},
                {.x = 142247.93604, .y = 55699.399162},
                {.x = 152247.93604, .y = 95699.399162},
                {.x = 142247.93604, .y = 115699.399162},
                {.x = 92247.93604, .y = 120699.399162},
                {.x = 22247.93604, .y = 105699.399162},
                {.x = 247.93604, .y = 75699.399162},
            },
        .z_range = ZRange{.min = -16000.0, .max = 60000.0},
    },
    {
        .world_id = "8000001",
        .polygon =
            {
                {.x = -19998.51, .y = -17867.29},
                {.x = 25873.72, .y = -17867.29},
                {.x = 25873.72, .y = 27999.89},
                {.x = -19998.51, .y = 27999.89},
            },
        .z_range = ZRange{.min = -3500.0, .max = 10200.0},
    },
};

auto trim_ascii_copy(std::string_view value) -> std::string {
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };

  std::size_t start = 0;
  while (start < value.size() && is_space(static_cast<unsigned char>(value[start]))) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && is_space(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }

  return std::string(value.substr(start, end - start));
}

auto normalize_world_id(std::string_view world_id) -> std::string {
  auto normalized_world_id = trim_ascii_copy(world_id);
  if (const auto dot_pos = normalized_world_id.find('.'); dot_pos != std::string::npos) {
    normalized_world_id = normalized_world_id.substr(0, dot_pos);
  }
  return normalized_world_id;
}

struct BoundingBox {
  double min_x = 0.0;
  double max_x = 0.0;
  double min_y = 0.0;
  double max_y = 0.0;
};

struct CompiledWorldPolygonRule {
  const WorldPolygonRule* rule = nullptr;
  BoundingBox bbox{};
};

auto build_bbox(const std::vector<PolygonPoint>& polygon) -> std::optional<BoundingBox> {
  if (polygon.empty()) {
    return std::nullopt;
  }

  BoundingBox box{
      .min_x = polygon.front().x,
      .max_x = polygon.front().x,
      .min_y = polygon.front().y,
      .max_y = polygon.front().y,
  };

  for (const auto& point : polygon) {
    box.min_x = std::min(box.min_x, point.x);
    box.max_x = std::max(box.max_x, point.x);
    box.min_y = std::min(box.min_y, point.y);
    box.max_y = std::max(box.max_y, point.y);
  }

  return box;
}

auto point_in_bbox(double x, double y, const BoundingBox& bbox) -> bool {
  return x >= bbox.min_x && x <= bbox.max_x && y >= bbox.min_y && y <= bbox.max_y;
}

auto get_compiled_rules() -> const std::vector<CompiledWorldPolygonRule>& {
  static const std::vector<CompiledWorldPolygonRule> compiled_rules = [] {
    std::vector<CompiledWorldPolygonRule> result;
    result.reserve(kWorldPolygonRules.size());
    for (const auto& rule : kWorldPolygonRules) {
      const auto bbox = build_bbox(rule.polygon);
      if (!bbox.has_value()) {
        continue;
      }
      result.push_back(CompiledWorldPolygonRule{
          .rule = &rule,
          .bbox = *bbox,
      });
    }
    return result;
  }();

  return compiled_rules;
}

auto find_compiled_rule_by_world_id(std::string_view world_id) -> const CompiledWorldPolygonRule* {
  const auto normalized_world_id = normalize_world_id(world_id);
  if (normalized_world_id.empty()) {
    return nullptr;
  }

  for (const auto& compiled_rule : get_compiled_rules()) {
    if (compiled_rule.rule != nullptr && compiled_rule.rule->world_id == normalized_world_id) {
      return &compiled_rule;
    }
  }

  return nullptr;
}

auto is_point_in_polygon(double x, double y, const std::vector<PolygonPoint>& polygon) -> bool {
  if (polygon.size() < 3) {
    return false;
  }

  bool is_inside = false;
  for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
    const auto& current_point = polygon[i];
    const auto& previous_point = polygon[j];

    const auto yi = current_point.y;
    const auto xi = current_point.x;
    const auto yj = previous_point.y;
    const auto xj = previous_point.x;

    const bool intersects = (yi > y) != (yj > y) &&
                            x < ((xj - xi) * (y - yi)) / (yj - yi == 0.0 ? 1e-12 : (yj - yi)) + xi;
    if (intersects) {
      is_inside = !is_inside;
    }
  }

  return is_inside;
}

auto matches_z_range(std::optional<double> z, const std::optional<ZRange>& z_range) -> bool {
  if (!z_range.has_value()) {
    return true;
  }

  const bool has_min = z_range->min.has_value() && std::isfinite(*z_range->min);
  const bool has_max = z_range->max.has_value() && std::isfinite(*z_range->max);
  if (!has_min && !has_max) {
    return true;
  }

  if (!z.has_value() || !std::isfinite(*z)) {
    return false;
  }

  if (has_min && *z < *z_range->min) {
    return false;
  }
  if (has_max && *z > *z_range->max) {
    return false;
  }

  return true;
}

auto matches_rule(const GamePoint& point, const CompiledWorldPolygonRule& compiled_rule) -> bool {
  if (compiled_rule.rule == nullptr) {
    return false;
  }

  if (!point_in_bbox(point.x, point.y, compiled_rule.bbox)) {
    return false;
  }

  if (!is_point_in_polygon(point.x, point.y, compiled_rule.rule->polygon)) {
    return false;
  }

  return matches_z_range(point.z, compiled_rule.rule->z_range);
}

auto belongs_to_world(const GamePoint& point, std::string_view world_id) -> bool {
  const auto* compiled_rule = find_compiled_rule_by_world_id(world_id);
  if (compiled_rule == nullptr) {
    return false;
  }

  return matches_rule(point, *compiled_rule);
}

auto match_world_id(const GamePoint& point) -> std::optional<std::string> {
  for (const auto& compiled_rule : get_compiled_rules()) {
    if (!matches_rule(point, compiled_rule)) {
      continue;
    }
    return compiled_rule.rule->world_id;
  }

  return std::nullopt;
}

auto resolve_world_id_or_default(const GamePoint& point) -> std::string {
  const auto matched_world_id = match_world_id(point);
  if (matched_world_id.has_value()) {
    return *matched_world_id;
  }

  return std::string(kDefaultWorldId);
}

}  // namespace Extensions::InfinityNikki::WorldArea
