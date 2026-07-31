import assert from "node:assert/strict";
import { join } from "node:path";

import {
  copyScenarioFixture,
  findAllVisibleAssets,
  findFolderByPath,
  getFolderTree,
  runScenario,
  scanDirectory,
  waitForVisibleAsset,
} from "../support/support.ts";
import { canonicalizeWindowsPath } from "../support/runtime.ts";

// 中文、空格和特殊字符经过文件系统、JSON-RPC、SQLite 后仍应保持同一内部路径。
await runScenario("gallery/path_encoding", async (application, environment) => {
  const directoryName = "照片 图集";
  const fileName = "晚霞_%.png";
  const relativePath = join(directoryName, fileName);
  const assetPath = await copyScenarioFixture(environment, relativePath);

  await scanDirectory(application, environment.galleryDirectory);
  const asset = await waitForVisibleAsset(application, assetPath);

  assert.equal(canonicalizeWindowsPath(asset.path), canonicalizeWindowsPath(assetPath));
  assert.equal(asset.name, fileName, "Unicode 文件名经过 RPC 后发生变化");

  const folderPath = join(environment.galleryDirectory, directoryName);
  assert.ok(findFolderByPath(await getFolderTree(application), folderPath));

  // 重复扫描不能因路径中的 Unicode、空格、百分号或下划线产生第二条资产。
  await scanDirectory(application, environment.galleryDirectory);
  const matches = await findAllVisibleAssets(application, assetPath);
  assert.equal(matches.length, 1, "特殊路径重复扫描后产生重复资产");
  assert.equal(matches[0].id, asset.id);

  await application.restart();
  const afterRestart = await waitForVisibleAsset(application, assetPath);
  assert.equal(afterRestart.id, asset.id, "重启后 Unicode 路径资产身份发生变化");
  assert.equal(afterRestart.name, fileName);
});
