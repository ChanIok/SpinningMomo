#pragma once

namespace Features::Photography::State {

struct PhotographyState {
  std::atomic<bool> enabled{false};
  // Demo 阶段按采样帧数控制长曝光；0 表示关闭。
  std::atomic<int> shutter_frames{0};
};

}  // namespace Features::Photography::State
