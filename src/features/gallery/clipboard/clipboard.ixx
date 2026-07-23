module;

export module Features.Gallery.Clipboard;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Clipboard {

// 将选中资产解析为真实文件并写入系统剪贴板。
export auto copy_assets(Core::State::AppState& app_state, const std::vector<std::int64_t>& ids)
    -> std::expected<Types::OperationResult, std::string>;

// 将剪贴板文件或截图无覆盖地写入指定图库目录，再同步建立资产索引。
export auto paste_to_folder(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<Types::OperationResult, std::string>;

}  // namespace Features::Gallery::Clipboard
