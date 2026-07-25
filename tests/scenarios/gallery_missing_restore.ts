import assert from "node:assert/strict";
import { copyFile, rm } from "node:fs/promises";
import { join } from "node:path";

import {
  ApplicationHarness,
  REPOSITORY_ROOT,
  canonicalizeWindowsPath,
  createScenarioEnvironment,
  formatError,
  removeScenarioEnvironment,
  resolveExecutablePath,
  waitUntil,
  type ScenarioEnvironment,
} from "./runtime.ts";

type Asset = {
  id: number;
  path: string;
  hash?: string;
  description?: string;
};

type QueryAssetsResponse = {
  items: Asset[];
  totalCount: number;
};

type ScanResult = {
  errors: string[];
};

type OperationResult = {
  success: boolean;
  message: string;
  affectedCount?: number;
};

type MissingAsset = {
  id: number;
  path: string;
  missingAt: number;
};

type MissingAssetsResponse = {
  items: MissingAsset[];
  totalCount: number;
};

const DESCRIPTION = "scenario: missing path restored";

// 验证原路径消失后进入 missing，恢复时复用原资产并保留用户描述。
async function runScenario(): Promise<void> {
  const sourceExecutablePath = resolveExecutablePath(process.argv.slice(2));
  let environment: ScenarioEnvironment | undefined;
  let application: ApplicationHarness | undefined;

  try {
    console.log("[1/6] 创建隔离环境");
    environment = await createScenarioEnvironment(sourceExecutablePath);
    application = new ApplicationHarness(environment);

    console.log("[2/6] 启动 SpinningMomo");
    await application.start();

    console.log("[3/6] 扫描图片并写入用户描述");
    const fixturePath = join(REPOSITORY_ROOT, "web", "public", "logo_192x192.png");
    const assetPath = join(environment.galleryDirectory, "photo.png");
    await copyFile(fixturePath, assetPath);

    const firstScan = await application.call<ScanResult>("gallery.scanDirectory", {
      directory: environment.galleryDirectory,
    });
    assert.deepEqual(firstScan.errors, [], `首次扫描包含错误：${firstScan.errors.join("; ")}`);

    const originalAsset = await findVisibleAsset(application, assetPath);
    assert.ok(originalAsset, `首次扫描后未找到资产：${assetPath}`);
    assert.ok(originalAsset.id > 0, "首次扫描返回了无效资产 ID");
    assert.ok(originalAsset.hash, "首次扫描后资产缺少内容 hash");

    const updateResult = await application.call<OperationResult>(
      "gallery.updateAssetDescription",
      {
        assetId: originalAsset.id,
        description: DESCRIPTION,
      },
    );
    assert.equal(updateResult.success, true, `写入用户描述失败：${updateResult.message}`);

    const updatedAsset = await findVisibleAsset(application, assetPath);
    assert.equal(updatedAsset?.description, DESCRIPTION, "用户描述没有成功写入");

    console.log("[4/6] 删除原件并验证 missing");
    await rm(assetPath);

    const missingScan = await application.call<ScanResult>("gallery.scanDirectory", {
      directory: environment.galleryDirectory,
    });
    assert.deepEqual(
      missingScan.errors,
      [],
      `删除后的扫描包含错误：${missingScan.errors.join("; ")}`,
    );

    const missingAsset = await waitUntil(
      "原资产进入 missing",
      async () => {
        const missing = await application.call<MissingAssetsResponse>(
          "gallery.getMissingAssets",
          {},
        );
        return findMissingAsset(missing, assetPath);
      },
      { timeoutMs: 10_000, intervalMs: 100 },
    );
    assert.equal(missingAsset.id, originalAsset.id, "进入 missing 后资产 ID 发生变化");
    assert.ok(missingAsset.missingAt > 0, "missing 资产缺少有效的 missingAt");
    assert.equal(
      await findVisibleAsset(application, assetPath),
      undefined,
      "missing 资产仍出现在普通图库查询中",
    );

    console.log("[5/6] 恢复原件并验证资产身份");
    await copyFile(fixturePath, assetPath);

    const restoreScan = await application.call<ScanResult>("gallery.scanDirectory", {
      directory: environment.galleryDirectory,
    });
    assert.deepEqual(
      restoreScan.errors,
      [],
      `恢复后的扫描包含错误：${restoreScan.errors.join("; ")}`,
    );

    const restoredAsset = await waitUntil(
      "原资产恢复可见并离开 missing",
      async () => {
        const [visibleAsset, missing] = await Promise.all([
          findVisibleAsset(application, assetPath),
          application.call<MissingAssetsResponse>("gallery.getMissingAssets", {}),
        ]);

        const isStillMissing = findMissingAsset(missing, assetPath) !== undefined;
        return visibleAsset && !isStillMissing ? visibleAsset : undefined;
      },
      { timeoutMs: 10_000, intervalMs: 100 },
    );

    assert.equal(restoredAsset.id, originalAsset.id, "恢复原路径后没有复用原资产 ID");
    assert.equal(restoredAsset.hash, originalAsset.hash, "恢复后的内容 hash 与原资产不同");
    assert.equal(restoredAsset.description, DESCRIPTION, "恢复原路径后用户描述丢失");

    const visibleMatches = await findAllVisibleAssets(application, assetPath);
    assert.equal(visibleMatches.length, 1, "恢复后同一路径产生了重复的可见资产");

    console.log("[6/6] 正常退出");
    await application.stop();
    await removeScenarioEnvironment(environment);
    console.log("PASS gallery_missing_restore");
  } catch (error) {
    if (application) {
      try {
        await application.stop();
      } catch (stopError) {
        console.error(`退出测试程序时再次失败：\n${formatError(stopError)}`);
      }
    }

    console.error(`FAIL gallery_missing_restore\n${formatError(error)}`);
    if (environment) {
      console.error(`失败现场已保留：${environment.rootDirectory}`);
      console.error(`应用日志：${environment.logPath}`);
    }
    throw error;
  }
}

// 查询普通图库并按规范化绝对路径筛选资产。
async function findAllVisibleAssets(
  application: ApplicationHarness,
  assetPath: string,
): Promise<Asset[]> {
  const response = await application.call<QueryAssetsResponse>("gallery.queryAssets", {
    filters: {},
  });
  const expectedPath = canonicalizeWindowsPath(assetPath);
  return response.items.filter(
    (asset) => canonicalizeWindowsPath(asset.path) === expectedPath,
  );
}

// 返回指定路径唯一的可见资产，重复资产直接作为场景失败。
async function findVisibleAsset(
  application: ApplicationHarness,
  assetPath: string,
): Promise<Asset | undefined> {
  const matches = await findAllVisibleAssets(application, assetPath);
  assert.ok(matches.length <= 1, `同一路径出现 ${matches.length} 条可见资产：${assetPath}`);
  return matches[0];
}

// 从 missing 维护列表中按规范化绝对路径查找资产。
function findMissingAsset(
  response: MissingAssetsResponse,
  assetPath: string,
): MissingAsset | undefined {
  const expectedPath = canonicalizeWindowsPath(assetPath);
  return response.items.find(
    (asset) => canonicalizeWindowsPath(asset.path) === expectedPath,
  );
}

await runScenario().catch(() => {
  process.exitCode = 1;
});
