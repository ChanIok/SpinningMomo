import assert from "node:assert/strict";
import { DatabaseSync } from "node:sqlite";

import {
  copyScenarioFixture,
  findFolderByPath,
  findVisibleAsset,
  getFolderTree,
  getMissingAssets,
  runScenario,
  scanDirectory,
  waitForVisibleAsset,
} from "../support/support.ts";
import { waitUntil } from "../support/runtime.ts";

// 通过数据库把已建立的本地 root 转成不可达 UNC root，模拟用户重启时的离线网络盘。
// 这样无需真的创建网络共享，也能验证启动恢复不会把整棵 root 批量标记 Missing。
await runScenario("gallery/unreachable_root", async (application, environment) => {
  const localAssetPath = await copyScenarioFixture(environment, "remote-root-photo.png");
  await scanDirectory(application, environment.galleryDirectory);

  const localAsset = await waitForVisibleAsset(application, localAssetPath);
  const rootFolder = findFolderByPath(
    await getFolderTree(application),
    environment.galleryDirectory,
  );
  assert.ok(rootFolder, "扫描后未找到本地 Gallery 根目录");

  await application.stop();

  const remoteRoot = `//spinning-momo-scenario-unreachable.invalid/share-${process.pid}`;
  const remoteAssetPath = `${remoteRoot}/remote-root-photo.png`;
  const database = new DatabaseSync(environment.databasePath);
  try {
    database
      .prepare("UPDATE folders SET path = ?, name = ? WHERE id = ?")
      .run(remoteRoot, "share", rootFolder.id);
    database
      .prepare("UPDATE assets SET path = ?, name = ? WHERE id = ?")
      .run(remoteAssetPath, "remote-root-photo.png", localAsset.id);
  } finally {
    database.close();
  }

  await application.restart();

  const remoteAsset = await waitUntil(
    "不可达 root 中的既有资产保持可见",
    async () => findVisibleAsset(application, remoteAssetPath),
    { timeoutMs: 10_000, intervalMs: 100, retryErrors: true },
  );
  assert.equal(remoteAsset.id, localAsset.id);

  const remoteFolder = findFolderByPath(await getFolderTree(application), remoteRoot);
  assert.ok(remoteFolder);
  assert.equal(remoteFolder.id, rootFolder.id);

  const missing = await getMissingAssets(application);
  assert.equal(
    missing.items.some((item) => item.id === localAsset.id),
    false,
    "不可达网络 root 在启动恢复时被错误标记为 Missing",
  );
});
