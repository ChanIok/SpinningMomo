module;

export module Extensions.InfinityNikki.WorldArea;

import std;
import Core.State;
import Extensions.InfinityNikki.Types;
import <asio.hpp>;

export namespace Extensions::InfinityNikki::WorldArea {

// 游戏内原始坐标（来自照片 EXIF 提取）。
struct GamePoint {
  double x = 0.0;
  double y = 0.0;
  std::optional<double> z;
};

// 经过坐标变换后的地图经纬度，可直接用于 Leaflet 等地图引擎。
struct MapCoordinate {
  double lat = 0.0;
  double lng = 0.0;
};

// 加载远端地图配置（带内存缓存）。首次调用会发起 HTTP 请求。
export auto load_map_config(Core::State::AppState& app_state) -> asio::awaitable<
    std::expected<Extensions::InfinityNikki::InfinityNikkiMapConfig, std::string>>;

// 按 world_id 精确查找世界配置（需先 normalize）。
export auto find_world(const Extensions::InfinityNikki::InfinityNikkiMapConfig& config,
                       std::string_view world_id)
    -> const Extensions::InfinityNikki::InfinityNikkiMapWorld*;
// 根据游戏坐标推断所属世界，未命中则返回默认世界。
export auto resolve_world_or_default(
    const Extensions::InfinityNikki::InfinityNikkiMapConfig& config, const GamePoint& point)
    -> const Extensions::InfinityNikki::InfinityNikkiMapWorld*;
// 将游戏坐标变换为地图经纬度（lat/lng）。
export auto transform_game_to_map_coordinates(
    const GamePoint& point, const Extensions::InfinityNikki::InfinityNikkiMapWorld& world)
    -> std::expected<MapCoordinate, std::string>;
export auto normalize_world_id(std::string_view world_id) -> std::string;

}  // namespace Extensions::InfinityNikki::WorldArea
