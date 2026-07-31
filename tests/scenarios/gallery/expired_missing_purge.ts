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
  waitForVisibleAsset,
} from "../support/support.ts";
import { waitUntil } from "../support/runtime.ts";

const MISSING_RETENTION_DAYS = 30;

// 启动时应删除超过宽限期的 Missing 资产，并清理不再被任何资产引用的缩略图。
await runScenario("gallery/expired_missing_purge", async (application, environment) => {
  const assetPath = await copyScenarioFixture(environment, "expired.png");
  await scanDirectory(application, environment.galleryDirectory);

  const asset = await waitForVisibleAsset(application, assetPath);
  assert.ok(asset.hash, "资产缺少内容 hash，无法验证缩略图清理");
  assert.ok(asset.hash.length >= 4, "资产 hash 长度不足以构造缩略图路径");

  const thumbnailPath = join(
    environment.thumbnailsDirectory,
    asset.hash.slice(0, 2),
    asset.hash.slice(2, 4),
    `${asset.hash}.webp`,
  );
  await waitUntil(
    "缩略图生成",
    async () => {
      await access(thumbnailPath);
      return true;
    },
    { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
  );

  await rm(assetPath);
  await scanDirectory(application, environment.galleryDirectory);
  const missingAsset = await waitForMissingAsset(application, assetPath);

  await application.stop();

  const database = new DatabaseSync(environment.databasePath);
  try {
    const expiredAt = Date.now() - (MISSING_RETENTION_DAYS + 1) * 24 * 60 * 60 * 1000;
    const updateResult = database
      .prepare("UPDATE assets SET missing_at = ? WHERE id = ?")
      .run(expiredAt, missingAsset.id);
    assert.equal(Number(updateResult.changes), 1, "没有成功把资产设置为过期 Missing");
  } finally {
    database.close();
  }

  await application.restart();

  const missingAfterRestart = await getMissingAssets(application);
  assert.equal(
    missingAfterRestart.items.some((item) => item.id === missingAsset.id),
    false,
    "重启后过期 Missing 资产仍然存在",
  );
  assert.equal(await findVisibleAsset(application, assetPath), undefined);

  const databaseAfterRestart = new DatabaseSync(environment.databasePath);
  try {
    const deletedRow = databaseAfterRestart
      .prepare("SELECT id FROM assets WHERE id = ?")
      .get(missingAsset.id);
    assert.equal(deletedRow, undefined, "过期 Missing 资产仍留在 assets 表");
  } finally {
    databaseAfterRestart.close();
  }

  await waitUntil(
    "清理过期资产对应的孤儿缩略图",
    async () => {
      try {
        await access(thumbnailPath);
        return undefined;
      } catch (error) {
        if ((error as NodeJS.ErrnoException).code === "ENOENT") {
          return true;
        }
        throw error;
      }
    },
    { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
  );
});
