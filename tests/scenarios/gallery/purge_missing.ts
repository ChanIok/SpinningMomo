import assert from "node:assert/strict";
import { DatabaseSync } from "node:sqlite";
import { access, rm } from "node:fs/promises";
import { join } from "node:path";

import {
  copyScenarioFixture,
  findVisibleAsset,
  getMissingAssets,
  runScenario,
  scanDirectory,
  waitForMissingAsset,
  waitForMissingAssetToDisappear,
  waitForVisibleAsset,
  type PurgeMissingAssetsResult,
  type ScenarioPhase,
} from "../support/support.ts";
import { isMainModule, waitUntil } from "../support/runtime.ts";

async function assertAssetRowGone(environment: { databasePath: string }, assetId: number) {
  await waitUntil(
    `资产 ${assetId} 已从数据库硬删除`,
    async () => {
      const database = new DatabaseSync(environment.databasePath);
      try {
        database.exec("PRAGMA busy_timeout = 5000;");
        const row = database.prepare("SELECT id FROM assets WHERE id = ?").get(assetId);
        return row === undefined ? true : undefined;
      } finally {
        database.close();
      }
    },
    { timeoutMs: 10_000, intervalMs: 100, retryErrors: true },
  );
}

async function waitForThumbnailPresence(
  environment: { thumbnailsDirectory: string },
  hash: string,
  expectedPresent: boolean,
  description: string,
) {
  const thumbnailPath = join(
    environment.thumbnailsDirectory,
    hash.slice(0, 2),
    hash.slice(2, 4),
    `${hash}.webp`,
  );
  await waitUntil(
    description,
    async () => {
      try {
        await access(thumbnailPath);
        return expectedPresent ? true : undefined;
      } catch (error) {
        if ((error as NodeJS.ErrnoException).code === "ENOENT") {
          return expectedPresent ? undefined : true;
        }
        throw error;
      }
    },
    { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
  );
}

const phase: ScenarioPhase = {
  name: "purge_missing",
  // 应用内主动 purge：只删除仍是 Missing 的资产（missing_at 二次检查），
  // 且缩略图按内容 hash 共享时不会被误判为孤儿。使用共享的 logo fixture。
  action: async (application, environment) => {
    const assetPath = await copyScenarioFixture(environment, "purge-me.png");
    await scanDirectory(application, environment.galleryDirectory);

    const asset = await waitForVisibleAsset(application, assetPath);
    assert.ok(asset.hash, "资产缺少内容 hash，无法验证缩略图保留");
    assert.ok(asset.hash.length >= 4, "资产 hash 长度不足以构造缩略图路径");
    await waitForThumbnailPresence(environment, asset.hash, true, "缩略图生成");

    await rm(assetPath);
    await scanDirectory(application, environment.galleryDirectory);
    const missingAsset = await waitForMissingAsset(application, assetPath);

    // 共享 hash 的 Missing 资产不会把缩略图标记为可回收。
    const missingBeforePurge = await getMissingAssets(application);
    assert.equal(missingBeforePurge.reclaimableThumbnailCount, 0, "共享缩略图被误判为可回收");

    const purgeResult = await application.call<PurgeMissingAssetsResult>(
      "gallery.purgeMissingAssets",
      { ids: [missingAsset.id] },
    );
    assert.equal(purgeResult.success, true, `purge 失败：${purgeResult.message}`);
    assert.equal(purgeResult.deletedAssetCount, 1, "purge 没有删除目标资产");
    assert.equal(purgeResult.skippedAssetCount, 0);
    assert.equal(purgeResult.deletedThumbnailCount, 0, "共享缩略图不应被 purge 删除");
    assert.equal(purgeResult.failedThumbnailCount, 0);

    await waitForMissingAssetToDisappear(application, assetPath);
    assert.equal(await findVisibleAsset(application, assetPath), undefined);
    await assertAssetRowGone(environment, missingAsset.id);
    await waitForThumbnailPresence(environment, asset.hash, true, "purge 后共享缩略图仍然保留");

    // missing_at 二次检查：已恢复可见的资产不应被 purge 误删。
    const restoredPath = await copyScenarioFixture(environment, "purge-restored.png");
    await scanDirectory(application, environment.galleryDirectory);
    const restoredBefore = await waitForVisibleAsset(application, restoredPath);

    await rm(restoredPath);
    await scanDirectory(application, environment.galleryDirectory);
    const restoredMissing = await waitForMissingAsset(application, restoredPath);

    await copyScenarioFixture(environment, "purge-restored.png");
    await scanDirectory(application, environment.galleryDirectory);
    const restoredAfter = await waitForVisibleAsset(application, restoredPath);
    assert.equal(restoredAfter.id, restoredBefore.id, "恢复的资产没有沿用原 ID");

    const skipResult = await application.call<PurgeMissingAssetsResult>(
      "gallery.purgeMissingAssets",
      { ids: [restoredMissing.id] },
    );
    assert.equal(skipResult.success, true, `purge 跳过失败：${skipResult.message}`);
    assert.equal(skipResult.deletedAssetCount, 0, "可见资产不应被 purge 删除");
    assert.equal(skipResult.skippedAssetCount, 1, "可见资产应被 purge 跳过");
    assert.equal((await findVisibleAsset(application, restoredPath))?.id, restoredAfter.id);
  },
};

export default phase;

if (isMainModule(import.meta.url)) {
  await runScenario("gallery/purge_missing", phase.action);
}
