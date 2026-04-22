module;

export module Extensions.InfinityNikki.WorldArea;

import std;

export namespace Extensions::InfinityNikki::WorldArea {

struct GamePoint {
  double x = 0.0;
  double y = 0.0;
  std::optional<double> z;
};

struct PolygonPoint {
  double x = 0.0;
  double y = 0.0;
};

struct ZRange {
  std::optional<double> min;
  std::optional<double> max;
};

struct WorldPolygonRule {
  std::string world_id;
  std::vector<PolygonPoint> polygon;
  std::optional<ZRange> z_range;
};

export auto belongs_to_world(const GamePoint& point, std::string_view world_id) -> bool;
export auto match_world_id(const GamePoint& point) -> std::optional<std::string>;
export auto resolve_world_id_or_default(const GamePoint& point) -> std::string;

}  // namespace Extensions::InfinityNikki::WorldArea
