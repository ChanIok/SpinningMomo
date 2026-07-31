import assert from "node:assert/strict";

import {
  assertOperationSuccess,
  copyScenarioFixture,
  createTag,
  findTagInTree,
  getAssetTags,
  runScenario,
  scanDirectory,
  waitForVisibleAsset,
  type ScenarioPhase,
  type TagStats,
  type TagTreeNode,
} from "../support/support.ts";
import { isMainModule } from "../support/runtime.ts";

const phase: ScenarioPhase = {
  name: "tag_crud",
  // 标签 CRUD、批量增删关系、统计口径（只计直接关联的可见资产）、删除级联清理关联。
  action: async (application, environment) => {
    const assetAPath = await copyScenarioFixture(environment, "tag-a.png");
    const assetBPath = await copyScenarioFixture(environment, "tag-b.png");
    await scanDirectory(application, environment.galleryDirectory);

    const assetA = await waitForVisibleAsset(application, assetAPath);
    const assetB = await waitForVisibleAsset(application, assetBPath);

    const tagAlpha = await createTag(application, "scenario-tag-alpha");
    const tagChild = await createTag(application, "scenario-tag-child", tagAlpha);

    let tree = await application.call<TagTreeNode[]>("gallery.getTagTree", {});
    const alphaNode = findTagInTree(tree, tagAlpha);
    assert.ok(alphaNode, "根标签不在标签树中");
    assert.equal(alphaNode.parentId ?? null, null, "根标签不应有父级");
    const childNode = findTagInTree(tree, tagChild);
    assert.ok(childNode, "子标签不在标签树中");
    assert.equal(childNode.parentId, tagAlpha, "子标签的父级关系错误");

    const renameResult = await application.call("gallery.updateTag", {
      id: tagChild,
      name: "scenario-tag-child-renamed",
    });
    assertOperationSuccess(renameResult, "重命名子标签");
    tree = await application.call<TagTreeNode[]>("gallery.getTagTree", {});
    assert.equal(findTagInTree(tree, tagChild)?.name, "scenario-tag-child-renamed");

    const addTagsResult = await application.call("gallery.addTagsToAsset", {
      assetId: assetA.id,
      tagIds: [tagAlpha, tagChild],
    });
    assertOperationSuccess(addTagsResult, "给单个资产添加多个标签");
    const batchAddResult = await application.call("gallery.addTagToAssets", {
      assetIds: [assetA.id, assetB.id],
      tagId: tagAlpha,
    });
    assertOperationSuccess(batchAddResult, "批量给多个资产添加标签");

    const assetATags = await getAssetTags(application, assetA.id);
    assert.ok(assetATags.some((tag) => tag.id === tagAlpha), "资产 A 缺少根标签");
    assert.ok(assetATags.some((tag) => tag.id === tagChild), "资产 A 缺少子标签");
    const assetBTags = await getAssetTags(application, assetB.id);
    assert.ok(assetBTags.some((tag) => tag.id === tagAlpha), "资产 B 缺少根标签");

    let stats = await application.call<TagStats[]>("gallery.getTagStats", {});
    assert.equal(
      stats.find((item) => item.tagId === tagAlpha)?.assetCount,
      2,
      "根标签直接关联统计错误",
    );
    assert.equal(
      stats.find((item) => item.tagId === tagChild)?.assetCount,
      1,
      "子标签直接关联统计错误",
    );

    const removeFromListResult = await application.call("gallery.removeTagFromAssets", {
      assetIds: [assetA.id],
      tagId: tagAlpha,
    });
    assertOperationSuccess(removeFromListResult, "从资产列表移除标签");
    const removeFromAssetResult = await application.call("gallery.removeTagsFromAsset", {
      assetId: assetB.id,
      tagIds: [tagAlpha],
    });
    assertOperationSuccess(removeFromAssetResult, "从单个资产移除多个标签");

    const assetAAfterRemove = await getAssetTags(application, assetA.id);
    assert.equal(
      assetAAfterRemove.some((tag) => tag.id === tagAlpha),
      false,
      "资产 A 移除后仍保留根标签",
    );
    assert.ok(assetAAfterRemove.some((tag) => tag.id === tagChild), "资产 A 的子标签被误移除");
    const assetBAfterRemove = await getAssetTags(application, assetB.id);
    assert.equal(
      assetBAfterRemove.some((tag) => tag.id === tagAlpha),
      false,
      "资产 B 移除后仍保留根标签",
    );

    const deleteChildResult = await application.call("gallery.deleteTag", { id: tagChild });
    assertOperationSuccess(deleteChildResult, "删除子标签");
    const afterChildDelete = await getAssetTags(application, assetA.id);
    assert.equal(
      afterChildDelete.some((tag) => tag.id === tagChild),
      false,
      "删除标签后资产关联仍然残留",
    );
    tree = await application.call<TagTreeNode[]>("gallery.getTagTree", {});
    assert.equal(findTagInTree(tree, tagChild), undefined, "被删除的子标签仍在标签树中");

    const cascadeParent = await createTag(application, "scenario-tag-cascade-parent");
    const cascadeChild = await createTag(application, "scenario-tag-cascade-child", cascadeParent);
    const deleteParentResult = await application.call("gallery.deleteTag", { id: cascadeParent });
    assertOperationSuccess(deleteParentResult, "删除父标签");
    tree = await application.call<TagTreeNode[]>("gallery.getTagTree", {});
    assert.equal(findTagInTree(tree, cascadeParent), undefined, "被删除的父标签仍在标签树中");
    assert.equal(
      findTagInTree(tree, cascadeChild),
      undefined,
      "删除父标签后子标签未级联删除",
    );

    const deleteAlphaResult = await application.call("gallery.deleteTag", { id: tagAlpha });
    assertOperationSuccess(deleteAlphaResult, "删除根标签");
    tree = await application.call<TagTreeNode[]>("gallery.getTagTree", {});
    assert.equal(findTagInTree(tree, tagAlpha), undefined, "被删除的根标签仍在标签树中");
    stats = await application.call<TagStats[]>("gallery.getTagStats", {});
    assert.equal(
      stats.some((item) => item.tagId === tagAlpha),
      false,
      "删除后统计仍包含该标签",
    );
  },
};

export default phase;

if (isMainModule(import.meta.url)) {
  await runScenario("gallery/tag_crud", phase.action);
}
