module;

export module Features.Photography.State;

import std;

namespace Features::Photography::State {

export struct PhotographyState {
  std::atomic<bool> enabled{false};
  // Demo 阶段按采样帧数控制长曝光；0 表示关闭。
  std::atomic<int> shutter_frames{0};
};

}  // namespace Features::Photography::State
