import folderTreePhase from "./gallery/folder_tree.ts";
import scannerMetadataPhase from "./gallery/scanner_metadata.ts";
import hashInheritancePhase from "./gallery/hash_inheritance.ts";
import moveConsistencyPhase from "./gallery/move_consistency.ts";
import missingRestorePhase from "./gallery/missing_restore.ts";
import watcherConsistencyPhase from "./gallery/watcher_consistency.ts";
import tagCrudPhase from "./gallery/tag_crud.ts";
import queryFiltersPhase from "./gallery/query_filters.ts";
import purgeMissingPhase from "./gallery/purge_missing.ts";

import { runScenarioPhases } from "./support/support.ts";

// 这些阶段都从同一空图库出发、使用互不冲突的文件路径、断言均按路径限定，
// 因此可以在同一次真实进程生命周期内顺序执行。
// hash_inheritance 依赖“最早的同 hash 资产”语义，使用独立 blue fixture；
// watcher_consistency 会留下 missing 资产；query_filters 断言以 qf- 前缀限定；
// purge_missing 依赖其余阶段留下的共享 logo 缩略图，放在最后。
await runScenarioPhases("gallery_core", [
  folderTreePhase,
  scannerMetadataPhase,
  hashInheritancePhase,
  moveConsistencyPhase,
  missingRestorePhase,
  watcherConsistencyPhase,
  tagCrudPhase,
  queryFiltersPhase,
  purgeMissingPhase,
]);
