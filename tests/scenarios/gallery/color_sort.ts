import assert from "node:assert/strict";
import { DatabaseSync } from "node:sqlite";

import {
  copyScenarioFixture,
  runScenario,
  scanDirectory,
  waitForVisibleAsset,
  type QueryAssetsResponse,
  type ScenarioPhase,
} from "../support/support.ts";
import { isMainModule } from "../support/runtime.ts";

const phase: ScenarioPhase = {
  name: "color_sort",
  action: async (application, environment) => {
    // 目标色为黑色；各主色按 [L, a, b, 占比] 指定，避免图片聚类误差影响断言。
    const fixtures = [
      {
        name: "large",
        colors: [
          [0, 10, 0, 0.6],
          [0, 80, 0, 0.4],
        ],
      },
      {
        name: "exact",
        colors: [
          [0, 0, 0, 0.05],
          [0, 80, 0, 0.95],
        ],
      },
      {
        name: "mixed",
        colors: [
          [0, 6, 0, 0.3],
          [0, 10, 0, 0.3],
          [0, 80, 0, 0.4],
        ],
      },
      { name: "wide-l", colors: [[50, 0, 0, 1]] },
      { name: "boundary-b", colors: [[0, 0, 60, 1]] },
      { name: "outside-a", colors: [[0, 61, 0, 1]] },
      { name: "no-colors", colors: [] },
    ];
    const paths = await Promise.all(
      fixtures.map(({ name }) => copyScenarioFixture(environment, `cs-${name}.png`)),
    );
    await scanDirectory(application, environment.galleryDirectory);
    const assets = await Promise.all(paths.map((path) => waitForVisibleAsset(application, path)));
    const ids = assets.map((asset) => asset.id);
    const [large, exact, mixed, wideL, boundary] = ids;

    const database = new DatabaseSync(environment.databasePath);
    try {
      database.exec("PRAGMA busy_timeout = 5000; BEGIN IMMEDIATE;");
      const removeColors = database.prepare("DELETE FROM asset_colors WHERE asset_id = ?");
      const insertColor = database.prepare(`
        INSERT INTO asset_colors (asset_id, r, g, b, lab_l, lab_a, lab_b, weight, l_bin, a_bin, b_bin)
        VALUES (?, 0, 0, 0, ?, ?, ?, ?, ?, ?, ?)
      `);
      for (const [index, fixture] of fixtures.entries()) {
        removeColors.run(ids[index]);
        for (const [l, a, b, weight] of fixture.colors) {
          insertColor.run(
            ids[index],
            l,
            a,
            b,
            weight,
            Math.floor(l / 5),
            Math.floor((a + 128) / 8),
            Math.floor((b + 128) / 8),
          );
        }
      }
      database.exec("COMMIT;");
    } finally {
      database.close();
    }

    const filters = { search: "cs-", colorHexes: ["#000000"], colorDistance: 18 };
    const query = (extra: Record<string, unknown> = {}) =>
      application.call<QueryAssetsResponse & { activeAssetIndex?: number }>("gallery.queryAssets", {
        filters,
        sortBy: "colorSimilarity",
        ...extra,
      });

    // 多主色贡献累加；大片相近色优先于小块精确色。
    const expectedIds = [mixed, large, exact];
    const response = await query({ activeAssetId: exact });
    assert.deepEqual(
      response.items.map((asset) => asset.id),
      expectedIds,
      "颜色加权排名不符",
    );
    assert.equal(response.totalCount, 3, "阈值外及没有主色的资产不应进入结果");
    assert.equal(response.activeAssetIndex, 2, "资产定位须遵循颜色排序");

    const firstPage = await query({ page: 1, perPage: 2 });
    const secondPage = await query({ page: 2, perPage: 2 });
    assert.deepEqual(
      [...firstPage.items, ...secondPage.items].map((asset) => asset.id),
      expectedIds,
      "分页须保持全局颜色排序",
    );
    const layout = await application.call<{ items: Array<{ id: number }> }>(
      "gallery.queryAssetLayoutMeta",
      { filters, sortBy: "colorSimilarity" },
    );
    assert.deepEqual(
      layout.items.map((item) => item.id),
      expectedIds,
      "布局须与查询顺序一致",
    );

    const wide = await query({ filters: { ...filters, colorDistance: 60 } });
    assert.deepEqual(
      wide.items.map((asset) => asset.id),
      [mixed, large, wideL, exact, boundary],
      "阈值 60 应包含远处分桶及边界颜色，排除色差 61",
    );
  },
};

export default phase;

if (isMainModule(import.meta.url)) {
  await runScenario("gallery/color_sort", phase.action);
}
