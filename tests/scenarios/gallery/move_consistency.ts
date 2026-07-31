import assert from "node:assert/strict";
import { access } from "node:fs/promises";
import { join } from "node:path";

import {
  assertOperationSuccess,
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

// 应用内移动必须同时完成磁盘移动、索引更新和 watcher 去重，并保留资产身份。
await runScenario("gallery/move_consistency", async (application, environment) => {
  const sourcePath = await copyScenarioFixture(environment, "move-source.png");
  await scanDirectory(application, environment.galleryDirectory);

  const sourceAsset = await waitForVisibleAsset(application, sourcePath);
  const descriptionResult = await application.call("gallery.updateAssetDescription", {
    assetId: sourceAsset.id,
    description: "scenario: moved asset",
  });
  assertOperationSuccess(descriptionResult, "写入待移动资产描述");

  const rootFolder = findFolderByPath(
    await getFolderTree(application),
    environment.galleryDirectory,
  );
  assert.ok(rootFolder, "扫描后未找到 Gallery 根目录");

  const targetName = "移动目标";
  const createFolderResult = await application.call("gallery.createFolder", {
    parentFolderId: rootFolder.id,
    name: targetName,
  });
  assertOperationSuccess(createFolderResult, "创建移动目标目录");
  const targetFolderPath = join(environment.galleryDirectory, targetName);
  const targetFolder = await waitUntil(
    "移动目标目录进入文件夹树",
    async () => findFolderByPath(await getFolderTree(application), targetFolderPath),
    { timeoutMs: 10_000, intervalMs: 100, retryErrors: true },
  );

  const movedPath = join(targetFolderPath, "move-source.png");
  const moveResult = await application.call("gallery.moveAssetsToFolder", {
    ids: [sourceAsset.id],
    targetFolderId: targetFolder.id,
  });
  assertOperationSuccess(moveResult, "移动资产");

  await access(movedPath);
  await waitForVisibleAsset(application, movedPath);
  assert.equal(await findVisibleAsset(application, sourcePath), undefined);

  const movedAsset = await waitForVisibleAsset(application, movedPath);
  assert.equal(movedAsset.id, sourceAsset.id, "移动后资产 ID 发生变化");
  assert.equal(movedAsset.folderId, targetFolder.id, "移动后 folder_id 不正确");
  assert.equal(movedAsset.description, "scenario: moved asset", "移动后用户描述丢失");

  const missing = await getMissingAssets(application);
  assert.equal(
    missing.items.some((item) => item.id === sourceAsset.id || item.path === sourcePath),
    false,
    "移动源路径被错误标记为 Missing",
  );

  // 再做一次全量扫描，确认手动操作产生的 ScanChange 没有留下重复行。
  await scanDirectory(application, environment.galleryDirectory);
  const afterScan = await waitForVisibleAsset(application, movedPath);
  assert.equal(afterScan.id, sourceAsset.id);
  assert.equal(await findVisibleAsset(application, sourcePath), undefined);
});
