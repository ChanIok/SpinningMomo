import settingsPersistencePhase from "./settings/persistence.ts";
import pathEncodingPhase from "./gallery/path_encoding.ts";
import expiredMissingPurgePhase from "./gallery/expired_missing_purge.ts";
import unreachableRootPhase from "./gallery/unreachable_root.ts";

import { runScenarioPhases } from "./support/support.ts";

// 共享一次进程生命周期，各自通过重启验证启动恢复逻辑。
// 顺序约束：expired_missing_purge 与 unreachable_root 都会停进程直接改库，
// 必须排在最后；unreachable_root 会把 root 改为不可达 UNC，不能再有场景依赖该 root。
// expired_missing_purge 依赖缩略图按 hash 共享语义，使用独立 green fixture。
await runScenarioPhases("gallery_recovery", [
  settingsPersistencePhase,
  pathEncodingPhase,
  expiredMissingPurgePhase,
  unreachableRootPhase,
]);
