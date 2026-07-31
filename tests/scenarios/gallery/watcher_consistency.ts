import assert from "node:assert/strict";
import { rename } from "node:fs/promises";

import {
  assertGallerySnapshotEqual,
  captureGallerySnapshot,
  copyScenarioFixture,
  runScenario,
  scanDirectory,
  waitForMissingAsset,
  waitForVisibleAsset,
  waitForVisibleAssetToDisappear,
  type ScenarioPhase,
} from "../support/support.ts";
import { isMainModule } from "../support/runtime.ts";

const phase: ScenarioPhase = {
  name: "watcher_consistency",
  // 文件系统 watcher 与全量 scanner 对同一批变化应收敛到相同的图库状态。
  action: async (application, environment) => {
    const originalPath = await copyScenarioFixture(environment, "watcher-original.png");
    await scanDirectory(application, environment.galleryDirectory);

    const originalAsset = await waitForVisibleAsset(application, originalPath);
    const renamedPath = originalPath.replace("watcher-original.png", "watcher-renamed.png");
    await rename(originalPath, renamedPath);
    const copiedPath = await copyScenarioFixture(environment, "watcher-copied.png");

    const renamedAsset = await waitForVisibleAsset(application, renamedPath);
    const copiedAsset = await waitForVisibleAsset(application, copiedPath);
    const missingOriginal = await waitForMissingAsset(application, originalPath);
    await waitForVisibleAssetToDisappear(application, originalPath);

    assert.notEqual(renamedAsset.id, originalAsset.id, "新路径错误地复用了旧路径资产 ID");
    assert.notEqual(copiedAsset.id, originalAsset.id, "复制路径错误地复用了旧路径资产 ID");
    assert.equal(missingOriginal.id, originalAsset.id);

    const watcherSnapshot = await captureGallerySnapshot(application);

    await scanDirectory(application, environment.galleryDirectory);
    await waitForVisibleAsset(application, renamedPath);
    await waitForVisibleAsset(application, copiedPath);
    await waitForMissingAsset(application, originalPath);
    const scannerSnapshot = await captureGallerySnapshot(application);

    assertGallerySnapshotEqual(
      scannerSnapshot,
      watcherSnapshot,
      "watcher 与全量 scanner 的最终图库快照不一致",
    );
  },
};

export default phase;

if (isMainModule(import.meta.url)) {
  await runScenario("gallery/watcher_consistency", phase.action);
}
