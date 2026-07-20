module;

export module Features.Gallery.Scanner;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Scanner {

// 全量同步一个目录：准备 → 发现 → 指纹判定 → 处理落库 → 清理 → 组装 ScanChange
export auto scan_asset_directory(
    Core::State::AppState& app_state, const Types::ScanOptions& options,
    std::function<void(const Types::ScanProgress&)> progress_callback = nullptr)
    -> std::expected<Types::ScanResult, std::string>;

}  // namespace Features::Gallery::Scanner
