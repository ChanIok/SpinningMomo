import assert from "node:assert/strict";

import {
  assertOperationSuccess,
  copyScenarioFixture,
  createTag,
  getAssetTags,
  runScenario,
  scanDirectory,
  waitForVisibleAsset,
  type ScenarioPhase,
} from "../support/support.ts";
import { isMainModule } from "../support/runtime.ts";

const phase: ScenarioPhase = {
  name: "hash_inheritance",
  // 同 hash 的新路径创建新资产，但只继承既有资产的用户字段与标签。
  // 依赖“最早的同 hash 资产”语义，因此必须使用内容唯一的 blue fixture，
  // 避免与套件内其他共享 logo hash 的场景互相污染。
  action: async (application, environment) => {
    const sourcePath = await copyScenarioFixture(environment, "source.png", "blue");
    await scanDirectory(application, environment.galleryDirectory);

    const sourceAsset = await waitForVisibleAsset(application, sourcePath);
    assert.ok(sourceAsset.hash, "源资产缺少内容 hash");

    const descriptionResult = await application.call("gallery.updateAssetDescription", {
      assetId: sourceAsset.id,
      description: "scenario: inherited metadata",
    });
    assertOperationSuccess(descriptionResult, "写入源资产描述");

    const reviewResult = await application.call("gallery.updateAssetsReviewState", {
      assetIds: [sourceAsset.id],
      rating: 5,
      reviewFlag: "picked",
    });
    assertOperationSuccess(reviewResult, "写入源资产审核字段");

    const inheritedTagId = await createTag(application, "scenario-inherited");
    const inheritedTagResult = await application.call("gallery.addTagsToAsset", {
      assetId: sourceAsset.id,
      tagIds: [inheritedTagId],
    });
    assertOperationSuccess(inheritedTagResult, "写入源资产标签");

    const enrichedSource = await waitForVisibleAsset(application, sourcePath);
    const duplicatePath = await copyScenarioFixture(environment, "duplicate.png", "blue");
    await scanDirectory(application, environment.galleryDirectory);

    const duplicateAsset = await waitForVisibleAsset(application, duplicatePath);
    assert.notEqual(duplicateAsset.id, enrichedSource.id, "新路径错误地复用了源资产 ID");
    assert.equal(duplicateAsset.hash, enrichedSource.hash, "相同内容没有得到相同 hash");
    assert.equal(duplicateAsset.description, "scenario: inherited metadata");
    assert.equal(duplicateAsset.rating, 5);
    assert.equal(duplicateAsset.reviewFlag, "picked");

    const duplicateTags = await getAssetTags(application, duplicateAsset.id);
    assert.ok(duplicateTags.some((tag) => tag.id === inheritedTagId), "新资产没有继承源标签");

    // 继承是创建时复制，不应让两个不同路径继续共享可变用户字段。
    const duplicateDescriptionResult = await application.call("gallery.updateAssetDescription", {
      assetId: duplicateAsset.id,
      description: "scenario: duplicate changed",
    });
    assertOperationSuccess(duplicateDescriptionResult, "更新重复资产描述");

    const extraTagId = await createTag(application, "scenario-duplicate-only");
    const extraTagResult = await application.call("gallery.addTagsToAsset", {
      assetId: duplicateAsset.id,
      tagIds: [extraTagId],
    });
    assertOperationSuccess(extraTagResult, "写入重复资产独有标签");

    const sourceAfterDuplicateUpdate = await waitForVisibleAsset(application, sourcePath);
    assert.equal(sourceAfterDuplicateUpdate.description, "scenario: inherited metadata");
    assert.equal(sourceAfterDuplicateUpdate.rating, 5);
    assert.equal(sourceAfterDuplicateUpdate.reviewFlag, "picked");

    const sourceTags = await getAssetTags(application, sourceAfterDuplicateUpdate.id);
    assert.ok(sourceTags.some((tag) => tag.id === inheritedTagId));
    assert.equal(
      sourceTags.some((tag) => tag.id === extraTagId),
      false,
      "重复资产的新标签反向污染了源资产",
    );
  },
};

export default phase;

if (isMainModule(import.meta.url)) {
  await runScenario("gallery/hash_inheritance", phase.action);
}
