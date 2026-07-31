import assert from "node:assert/strict";
import { join } from "node:path";

import {
  assertOperationSuccess,
  copyScenarioFixture,
  findFolderByPath,
  getFolderTree,
  runScenario,
  scanDirectory,
  waitForVisibleAsset,
} from "../support/support.ts";
import { waitUntil } from "../support/runtime.ts";

// 目录创建、目录树 parent 关系和扫描资产 folder_id 应保持同一棵树。
await runScenario("gallery/folder_tree", async (application, environment) => {
  await scanDirectory(application, environment.galleryDirectory);

  const rootFolder = await waitUntil(
    "Gallery 根目录进入文件夹树",
    async () => findFolderByPath(await getFolderTree(application), environment.galleryDirectory),
    { timeoutMs: 10_000, intervalMs: 100, retryErrors: true },
  );
  assert.equal(rootFolder.parentId ?? null, null);

  const childName = "场景子目录";
  const childResult = await application.call("gallery.createFolder", {
    parentFolderId: rootFolder.id,
    name: childName,
  });
  assertOperationSuccess(childResult, "创建 Unicode 子目录");
  const childPath = join(environment.galleryDirectory, childName);

  const childFolder = await waitUntil(
    "Unicode 子目录进入文件夹树",
    async () => findFolderByPath(await getFolderTree(application), childPath),
    { timeoutMs: 10_000, intervalMs: 100, retryErrors: true },
  );
  assert.equal(childFolder.parentId, rootFolder.id);

  const grandchildName = "更深一层";
  const grandchildResult = await application.call("gallery.createFolder", {
    parentFolderId: childFolder.id,
    name: grandchildName,
  });
  assertOperationSuccess(grandchildResult, "创建嵌套 Unicode 子目录");
  const grandchildPath = join(childPath, grandchildName);

  const grandchildFolder = await waitUntil(
    "嵌套目录进入文件夹树",
    async () => findFolderByPath(await getFolderTree(application), grandchildPath),
    { timeoutMs: 10_000, intervalMs: 100, retryErrors: true },
  );
  assert.equal(grandchildFolder.parentId, childFolder.id);

  const assetPath = await copyScenarioFixture(
    environment,
    join(childName, grandchildName, "nested-photo.png"),
  );
  await scanDirectory(application, environment.galleryDirectory);

  const asset = await waitForVisibleAsset(application, assetPath);
  assert.equal(asset.folderId, grandchildFolder.id, "扫描资产没有挂到最深层目录");

  const finalTree = await getFolderTree(application);
  const finalGrandchild = findFolderByPath(finalTree, grandchildPath);
  assert.ok(finalGrandchild);
  assert.equal(finalGrandchild.parentId, childFolder.id);
});
