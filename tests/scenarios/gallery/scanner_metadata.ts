import assert from "node:assert/strict";
import { utimes } from "node:fs/promises";

import {
  assertOperationSuccess,
  copyScenarioFixture,
  createTag,
  findVisibleAsset,
  getAssetTags,
  runScenario,
  scanDirectory,
  waitForVisibleAsset,
  type ScenarioPhase,
} from "../support/support.ts";
import { isMainModule, waitUntil } from "../support/runtime.ts";

const phase: ScenarioPhase = {
  name: "scanner_metadata",
  // 文件 mtime 变化应更新派生字段，但不能覆盖描述、评分、审核状态和标签。
  action: async (application, environment) => {
    const assetPath = await copyScenarioFixture(environment, "metadata.png");
    await scanDirectory(application, environment.galleryDirectory);

    const initialAsset = await waitForVisibleAsset(application, assetPath);
    const descriptionResult = await application.call("gallery.updateAssetDescription", {
      assetId: initialAsset.id,
      description: "scenario: scanner preserves metadata",
    });
    assertOperationSuccess(descriptionResult, "写入描述");

    const reviewResult = await application.call("gallery.updateAssetsReviewState", {
      assetIds: [initialAsset.id],
      rating: 3,
      reviewFlag: "rejected",
    });
    assertOperationSuccess(reviewResult, "写入审核字段");

    const tagId = await createTag(application, "scenario-scanner-preserve");
    const tagResult = await application.call("gallery.addTagsToAsset", {
      assetId: initialAsset.id,
      tagIds: [tagId],
    });
    assertOperationSuccess(tagResult, "写入标签");

    const beforeChange = await waitForVisibleAsset(application, assetPath);
    assert.ok(beforeChange.fileModifiedAt !== undefined && beforeChange.fileModifiedAt !== null);
    const previousModifiedAt = beforeChange.fileModifiedAt;

    const changedTime = new Date(Date.now() + 120_000);
    await utimes(assetPath, changedTime, changedTime);
    await scanDirectory(application, environment.galleryDirectory);

    const afterChange = await waitUntil(
      "scanner 更新文件 mtime",
      async () => {
        const asset = await findVisibleAsset(application, assetPath);
        return asset && asset.fileModifiedAt !== previousModifiedAt ? asset : undefined;
      },
      { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
    );

    assert.notEqual(afterChange.fileModifiedAt, previousModifiedAt);
    assert.equal(afterChange.id, beforeChange.id);
    assert.equal(afterChange.hash, beforeChange.hash, "仅修改 mtime 不应改变内容 hash");
    assert.equal(afterChange.description, "scenario: scanner preserves metadata");
    assert.equal(afterChange.rating, 3);
    assert.equal(afterChange.reviewFlag, "rejected");

    const tags = await getAssetTags(application, afterChange.id);
    assert.ok(tags.some((tag) => tag.id === tagId), "扫描后用户标签丢失");
  },
};

export default phase;

if (isMainModule(import.meta.url)) {
  await runScenario("gallery/scanner_metadata", phase.action);
}
