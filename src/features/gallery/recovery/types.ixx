module;

export module Features.Gallery.Recovery.Types;

import std;
import Features.Gallery.Types;

namespace Features::Gallery::Recovery::Types {

// 持久化到 DB 的恢复检查点，记录"下次启动时从哪里开始读 USN Journal"。
export struct WatchRootRecoveryState {
  std::string root_path;                       // 监视的根目录路径
  std::string volume_identity;                 // 卷标识（如 "ntfs:ABCD1234"），用于检测磁盘更换
  std::optional<std::int64_t> journal_id;      // USN Journal ID，Journal 被重建时会变化
  std::optional<std::int64_t> checkpoint_usn;  // 上次读到的 USN 位置
  std::string rule_fingerprint;                // 扫描规则指纹，规则变化时需要全量重扫
  std::int64_t updated_at = 0;
};

// 启动恢复模式
export enum class StartupRecoveryMode {
  UsnJournal,  // 读取 USN Journal 增量恢复离线期间的变更
  FullScan,    // 无法增量恢复，回退到全量扫描
};

// 启动恢复决策结果。由 recovery service 生成，watcher 消费。
// 同时携带当前卷快照信息，watcher 可直接用于 persist 而无需重新查询。
export struct StartupRecoveryPlan {
  StartupRecoveryMode mode = StartupRecoveryMode::FullScan;
  std::string reason;                                         // 决策原因（用于日志）
  std::vector<Features::Gallery::Types::ScanChange> changes;  // USN 模式下收集到的离线变更
  // 以下字段从当前卷快照中填充，供 watcher 在恢复完成后直接持久化检查点。
  std::string root_path;
  std::string volume_identity;
  std::string rule_fingerprint;
  std::optional<std::int64_t> journal_id;
  std::optional<std::int64_t> checkpoint_usn;
};

}  // namespace Features::Gallery::Recovery::Types
