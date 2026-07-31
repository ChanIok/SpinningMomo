import assert from "node:assert/strict";
import { rm } from "node:fs/promises";

import {
  copyScenarioFixture,
  findMissingAsset,
  findVisibleAsset,
  getMissingAssets,
  runScenario,
  scanDirectory,
  waitForMissingAsset,
  waitForVisibleAsset,
  type ScenarioPhase,
} from "../support/support.ts";
import { isMainModule } from "../support/runtime.ts";

const DESCRIPTION = "scenario: missing path restored";

const phase: ScenarioPhase = {
  name: "missing_restore",
  // 验证原路径消失后进入 missing，恢复时复用原资产并保留用户描述。
  action: async (application, environment) => {
    const assetPath = await copyScenarioFixture(environment, "photo.png");
    await scanDirectory(application, environment.galleryDirectory);

    const originalAsset = await findVisibleAsset(application, assetPath);
    assert.ok(originalAsset, `首次扫描后未找到资产：${assetPath}`);
    assert.ok(originalAsset.id > 0, "首次扫描返回了无效资产 ID");
    assert.ok(originalAsset.hash, "首次扫描后资产缺少内容 hash");

    const updateResult = await application.call<{ success: boolean; message: string }>(
      "gallery.updateAssetDescription",
      {
        assetId: originalAsset.id,
        description: DESCRIPTION,
      },
    );
    assert.equal(updateResult.success, true, `写入用户描述失败：${updateResult.message}`);

    const updatedAsset = await findVisibleAsset(application, assetPath);
    assert.equal(updatedAsset?.description, DESCRIPTION, "用户描述没有成功写入");

    await rm(assetPath);
    await scanDirectory(application, environment.galleryDirectory);

    const missingAsset = await waitForMissingAsset(application, assetPath);
    assert.equal(missingAsset.id, originalAsset.id, "进入 missing 后资产 ID 发生变化");
    assert.ok(missingAsset.missingAt > 0, "missing 资产缺少有效的 missingAt");
    assert.equal(
      await findVisibleAsset(application, assetPath),
      undefined,
      "missing 资产仍出现在普通图库查询中",
    );

    await copyScenarioFixture(environment, "photo.png");
    await scanDirectory(application, environment.galleryDirectory);

    const restoredAsset = await waitForVisibleAsset(application, assetPath);
    assert.equal(restoredAsset.id, originalAsset.id, "恢复原路径后没有复用原资产 ID");
    assert.equal(restoredAsset.hash, originalAsset.hash, "恢复后的内容 hash 与原资产不同");
    assert.equal(restoredAsset.description, DESCRIPTION, "恢复原路径后用户描述丢失");

    const missingAfterRestore = await getMissingAssets(application);
    assert.equal(
      findMissingAsset(missingAfterRestore, assetPath),
      undefined,
      "恢复原路径后资产未离开 missing 列表",
    );
  },
};

export default phase;

if (isMainModule(import.meta.url)) {
  await runScenario("gallery/missing_restore", phase.action);
}
