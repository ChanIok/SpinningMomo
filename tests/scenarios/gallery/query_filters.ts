import assert from "node:assert/strict";
import { join } from "node:path";

import {
  assertOperationSuccess,
  copyScenarioFixture,
  createTag,
  findFolderByPath,
  getFolderTree,
  queryAssets,
  runScenario,
  scanDirectory,
  waitForVisibleAsset,
  type Asset,
  type BatchSelectionSummary,
  type HomeStats,
  type QueryAssetsResponse,
  type ScenarioPhase,
} from "../support/support.ts";
import { canonicalizeWindowsPath, isMainModule, waitUntil } from "../support/runtime.ts";

function pathsOf(response: QueryAssetsResponse): string[] {
  return response.items.map((item) => canonicalizeWindowsPath(item.path));
}

async function waitForMetadata(
  application: Parameters<typeof queryAssets>[0],
  predicate: (assets: Asset[]) => boolean,
  description: string,
): Promise<Asset[]> {
  return waitUntil(
    description,
    async () => {
      const response = await queryAssets(application, { search: "qf-" });
      return predicate(response.items) ? response.items : undefined;
    },
    { timeoutMs: 10_000, intervalMs: 100, retryErrors: true },
  );
}

const phase: ScenarioPhase = {
  name: "query_filters",
  // 统一查询的筛选、排序、分页，以及 home stats / 批量摘要 / 时间线的联动。
  // 断言都以 qf- 前缀限定到本阶段创建的资产，不依赖图库的全局数量。
  action: async (application, environment) => {
    const beforeStats = await application.call<HomeStats>("gallery.getHomeStats", {});

    const assetAPath = await copyScenarioFixture(environment, "qf-a.png");
    const assetBPath = await copyScenarioFixture(environment, "qf-b.png");
    const assetCPath = await copyScenarioFixture(environment, "qf-c.png");
    const assetDPath = await copyScenarioFixture(environment, join("qf-sub", "qf-d.png"));
    await scanDirectory(application, environment.galleryDirectory);

    const assetA = await waitForVisibleAsset(application, assetAPath);
    const assetB = await waitForVisibleAsset(application, assetBPath);
    const assetC = await waitForVisibleAsset(application, assetCPath);
    const assetD = await waitForVisibleAsset(application, assetDPath);

    const setReview = async (assetId: number, rating: number, reviewFlag: string) => {
      const result = await application.call("gallery.updateAssetsReviewState", {
        assetIds: [assetId],
        rating,
        reviewFlag,
      });
      assertOperationSuccess(result, `写入资产 ${assetId} 审核字段`);
    };
    const setDescription = async (assetId: number, description: string) => {
      const result = await application.call("gallery.updateAssetDescription", {
        assetId,
        description,
      });
      assertOperationSuccess(result, `写入资产 ${assetId} 描述`);
    };

    await setReview(assetA.id, 5, "picked");
    await setReview(assetB.id, 5, "rejected");
    await setReview(assetC.id, 1, "none");
    await setDescription(assetA.id, "scenario: qf batch");
    await setDescription(assetB.id, "scenario: qf batch");
    await setDescription(assetC.id, "scenario: qf solo");

    const tagCommon = await createTag(application, "scenario-qf-tag-common");
    const tagSolo = await createTag(application, "scenario-qf-tag-solo");
    for (const [assetId, tagId] of [
      [assetA.id, tagCommon],
      [assetB.id, tagCommon],
      [assetC.id, tagSolo],
    ] as const) {
      const result = await application.call("gallery.addTagsToAsset", {
        assetId,
        tagIds: [tagId],
      });
      assertOperationSuccess(result, `给资产 ${assetId} 添加标签`);
    }

    // 等待元数据落库后，再按统一查询口径验证。
    await waitForMetadata(
      application,
      (assets) =>
        assets.some((item) => item.rating === 5 && item.reviewFlag === "picked") &&
        assets.some((item) => item.description === "scenario: qf solo"),
      "资产元数据可见",
    );

    // 月份/年份按资产实际 file_created_at 推算，避免跨月边界时的时区抖动。
    const monthReference = assetD.fileCreatedAt ?? assetD.createdAt;
    assert.ok(monthReference, "资产缺少时间戳用于推算月份");
    const expectedMonth = new Date(monthReference).toISOString().slice(0, 7);
    const expectedYear = new Date(monthReference).toISOString().slice(0, 4);
    const aPath = canonicalizeWindowsPath(assetAPath);
    const bPath = canonicalizeWindowsPath(assetBPath);
    const cPath = canonicalizeWindowsPath(assetCPath);
    const dPath = canonicalizeWindowsPath(assetDPath);

    const sortByName = await queryAssets(application, { search: "qf-" }, { sortBy: "name", sortOrder: "asc" });
    assert.equal(sortByName.totalCount, 4, "qf- 前缀搜索应命中 4 个资产");
    assert.deepEqual(
      sortByName.items.map((item) => item.name),
      ["qf-a.png", "qf-b.png", "qf-c.png", "qf-d.png"],
      "按名称升序排序结果不符",
    );

    const ratingsFive = await queryAssets(application, { search: "qf-", ratings: [5] });
    assert.deepEqual(new Set(pathsOf(ratingsFive)), new Set([aPath, bPath]));

    const ratingsOne = await queryAssets(application, { search: "qf-", ratings: [1] });
    assert.deepEqual(pathsOf(ratingsOne), [cPath]);

    const ratingsMixed = await queryAssets(application, { search: "qf-", ratings: [1, 5] });
    assert.deepEqual(new Set(pathsOf(ratingsMixed)), new Set([aPath, bPath, cPath]));

    const picked = await queryAssets(application, { search: "qf-", reviewFlag: "picked" });
    assert.deepEqual(pathsOf(picked), [aPath]);
    const rejected = await queryAssets(application, { search: "qf-", reviewFlag: "rejected" });
    assert.deepEqual(pathsOf(rejected), [bPath]);
    const unflagged = await queryAssets(application, { search: "qf-", reviewFlag: "none" });
    assert.deepEqual(new Set(pathsOf(unflagged)), new Set([cPath, dPath]));

    const byCommonTag = await queryAssets(application, { search: "qf-", tagIds: [tagCommon] });
    assert.deepEqual(new Set(pathsOf(byCommonTag)), new Set([aPath, bPath]));
    const bySoloTag = await queryAssets(application, { search: "qf-", tagIds: [tagSolo] });
    assert.deepEqual(pathsOf(bySoloTag), [cPath]);
    const anyTag = await queryAssets(application, {
      search: "qf-",
      tagIds: [tagCommon, tagSolo],
      tagMatchMode: "any",
    });
    assert.deepEqual(new Set(pathsOf(anyTag)), new Set([aPath, bPath, cPath]));
    const allTags = await queryAssets(application, {
      search: "qf-",
      tagIds: [tagCommon, tagSolo],
      tagMatchMode: "all",
    });
    assert.equal(allTags.totalCount, 0, "没有资产同时拥有两个标签");

    const photos = await queryAssets(application, { search: "qf-", type: "photo" });
    assert.equal(photos.totalCount, 4);
    const videos = await queryAssets(application, { search: "qf-", type: "video" });
    assert.equal(videos.totalCount, 0);

    const byMonth = await queryAssets(application, { search: "qf-", month: expectedMonth });
    assert.equal(byMonth.totalCount, 4, "月份筛选未命中全部资产");
    const byYear = await queryAssets(application, { search: "qf-", year: expectedYear });
    assert.equal(byYear.totalCount, 4, "年份筛选未命中全部资产");

    const rootFolder = await waitUntil(
      "Gallery 根目录进入文件夹树",
      async () => findFolderByPath(await getFolderTree(application), environment.galleryDirectory),
      { timeoutMs: 10_000, intervalMs: 100, retryErrors: true },
    );
    const rootOnly = await queryAssets(application, {
      search: "qf-",
      folderId: rootFolder.id,
      includeSubfolders: false,
    });
    assert.deepEqual(new Set(pathsOf(rootOnly)), new Set([aPath, bPath, cPath]));
    const rootWithSub = await queryAssets(application, {
      search: "qf-",
      folderId: rootFolder.id,
      includeSubfolders: true,
    });
    assert.deepEqual(new Set(pathsOf(rootWithSub)), new Set([aPath, bPath, cPath, dPath]));

    const firstPage = await queryAssets(
      application,
      { search: "qf-" },
      { sortBy: "name", sortOrder: "asc", page: 1, perPage: 2 },
    );
    assert.deepEqual(
      firstPage.items.map((item) => item.name),
      ["qf-a.png", "qf-b.png"],
      "第一页排序结果不符",
    );
    assert.equal(firstPage.totalCount, 4);
    assert.equal(firstPage.totalPages, 2);
    assert.equal(firstPage.currentPage, 1);
    assert.equal(firstPage.perPage, 2);
    const secondPage = await queryAssets(
      application,
      { search: "qf-" },
      { sortBy: "name", sortOrder: "asc", page: 2, perPage: 2 },
    );
    assert.deepEqual(
      secondPage.items.map((item) => item.name),
      ["qf-c.png", "qf-d.png"],
      "第二页排序结果不符",
    );

    const afterStats = await application.call<HomeStats>("gallery.getHomeStats", {});
    assert.equal(
      afterStats.totalCount - beforeStats.totalCount,
      4,
      "home stats 总数没有反映本阶段新增的资产",
    );
    assert.equal(
      afterStats.photoCount - beforeStats.photoCount,
      4,
      "home stats 照片数没有反映本阶段新增的资产",
    );

    const commonSummary = await application.call<BatchSelectionSummary>(
      "gallery.getBatchSelectionSummary",
      { assetIds: [assetA.id, assetB.id] },
    );
    assert.equal(commonSummary.selectedCount, 2);
    assert.equal(commonSummary.rating, 5, "批量摘要未合并相同评分");
    assert.equal(commonSummary.description, "scenario: qf batch", "批量摘要未合并相同描述");
    assert.ok(
      commonSummary.commonTags.some((tag) => tag.id === tagCommon),
      "批量摘要缺少共同标签",
    );
    const mixedSummary = await application.call<BatchSelectionSummary>(
      "gallery.getBatchSelectionSummary",
      { assetIds: [assetA.id, assetC.id] },
    );
    assert.equal(mixedSummary.selectedCount, 2);
    assert.equal(mixedSummary.rating, undefined, "评分不同时不应给出摘要评分");
    assert.equal(mixedSummary.description, undefined, "描述不同时不应给出摘要描述");
    assert.deepEqual(mixedSummary.commonTags, [], "无共同标签时不应给出共同标签");

    const timeline = await application.call<{
      buckets: Array<{ month: string; count: number }>;
      totalCount: number;
    }>("gallery.getTimelineBuckets", { search: "qf-" });
    assert.equal(timeline.totalCount, 4);
    const monthBucket = timeline.buckets.find((bucket) => bucket.month === expectedMonth);
    assert.ok(monthBucket, `时间线缺少当前月份桶 ${expectedMonth}`);
    assert.equal(monthBucket.count, 4, "时间线当前月份桶数量不符");

    const byMonthAssets = await application.call<{
      month: string;
      assets: Asset[];
      count: number;
    }>("gallery.getAssetsByMonth", { month: expectedMonth, search: "qf-" });
    assert.equal(byMonthAssets.count, 4);
    assert.deepEqual(
      new Set(byMonthAssets.assets.map((item) => canonicalizeWindowsPath(item.path))),
      new Set([aPath, bPath, cPath, dPath]),
    );
  },
};

export default phase;

if (isMainModule(import.meta.url)) {
  await runScenario("gallery/query_filters", phase.action);
}
