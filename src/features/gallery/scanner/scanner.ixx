module;

export module Features.Gallery.Scanner;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Scanner {

// 全量同步一个目录：准备 → 盘点文件/目录 → 同步目录库存 → 处理资产 → 清理 → 组装 ScanChange。
export auto scan_asset_directory(
    Core::State::AppState& app_state, const Types::ScanOptions& options,
    std::function<void(const Types::ScanProgress&)> progress_callback = nullptr)
    -> std::expected<Types::ScanResult, std::string>;

// 同步物化应用主动创建的单个文件，并返回已经落库的真实扫描变化。
export auto upsert_created_file(Core::State::AppState& app_state, std::int64_t folder_id,
                                const std::filesystem::path& path)
    -> std::expected<Types::ScanResult, std::string>;

}  // namespace Features::Gallery::Scanner
