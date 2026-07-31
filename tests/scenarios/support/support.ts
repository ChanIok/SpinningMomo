import assert from "node:assert/strict";
import { copyFile, mkdir, readFile } from "node:fs/promises";
import { join } from "node:path";

import {
  ApplicationHarness,
  REPOSITORY_ROOT,
  canonicalizeWindowsPath,
  createScenarioEnvironment,
  formatError,
  removeScenarioEnvironment,
  resolveExecutablePath,
  type ScenarioEnvironment,
  waitUntil,
} from "./runtime.ts";

export type Asset = {
  id: number;
  name?: string;
  path: string;
  type?: string;
  hash?: string | null;
  rating?: number;
  reviewFlag?: string;
  description?: string | null;
  size?: number | null;
  folderId?: number | null;
  fileModifiedAt?: number | null;
};

export type QueryAssetsResponse = {
  items: Asset[];
  totalCount: number;
  currentPage?: number;
  perPage?: number;
  totalPages?: number;
};

export type ScanResult = {
  totalFiles?: number;
  newItems?: number;
  updatedItems?: number;
  missingItems?: number;
  errors: string[];
};

export type OperationResult = {
  success: boolean;
  message: string;
  affectedCount?: number;
  failedCount?: number;
  notFoundCount?: number;
  unchangedCount?: number;
};

export type MissingAsset = {
  id: number;
  name: string;
  path: string;
  missingAt: number;
};

export type MissingAssetsResponse = {
  items: MissingAsset[];
  totalCount: number;
  reclaimableThumbnailCount: number;
  reclaimableThumbnailBytes: number;
};

export type Tag = {
  id: number;
  name: string;
};

export type FolderTreeNode = {
  id: number;
  path: string;
  parentId?: number | null;
  name: string;
  children: FolderTreeNode[];
};

export type GallerySnapshot = {
  visible: Array<{
    id: number;
    path: string;
    hash?: string | null;
    description?: string | null;
    rating?: number;
    reviewFlag?: string;
    fileModifiedAt?: number | null;
  }>;
  missing: Array<{
    id: number;
    path: string;
    missingAt: number;
  }>;
};

export type ScenarioAction = (
  application: ApplicationHarness,
  environment: ScenarioEnvironment,
) => Promise<void>;

// 统一管理单个场景的真实进程和一次性便携沙箱；失败时保留现场供诊断。
export async function runScenario(name: string, action: ScenarioAction): Promise<void> {
  const sourceExecutablePath = resolveExecutablePath(process.argv.slice(2));
  let environment: ScenarioEnvironment | undefined;
  let application: ApplicationHarness | undefined;

  try {
    console.log(`[${name}] 创建隔离环境`);
    environment = await createScenarioEnvironment(sourceExecutablePath);
    application = new ApplicationHarness(environment);

    console.log(`[${name}] 启动 SpinningMomo`);
    await application.start();
    await action(application, environment);

    await application.stop();
    await removeScenarioEnvironment(environment);
    console.log(`PASS ${name}`);
  } catch (error) {
    if (application) {
      try {
        await application.stop();
      } catch (stopError) {
        console.error(`退出测试程序时再次失败：\n${formatError(stopError)}`);
      }
    }

    console.error(`FAIL ${name}\n${formatError(error)}`);
    if (environment) {
      console.error(`失败现场已保留：${environment.rootDirectory}`);
      console.error(`应用日志：${environment.logPath}`);
    }
    throw error;
  }
}

export function scenarioFixturePath(): string {
  return join(REPOSITORY_ROOT, "web", "public", "logo_192x192.png");
}

export async function copyScenarioFixture(
  environment: ScenarioEnvironment,
  relativePath: string,
): Promise<string> {
  const destination = join(environment.galleryDirectory, relativePath);
  await mkdir(join(destination, ".."), { recursive: true });
  await copyFile(scenarioFixturePath(), destination);
  return destination;
}

export async function queryVisibleAssets(
  application: ApplicationHarness,
): Promise<Asset[]> {
  const response = await application.call<QueryAssetsResponse>("gallery.queryAssets", {
    filters: {},
  });
  return response.items;
}

export async function scanDirectory(
  application: ApplicationHarness,
  directory: string,
): Promise<ScanResult> {
  const result = await application.call<ScanResult>("gallery.scanDirectory", { directory });
  assert.deepEqual(result.errors, [], `扫描包含错误：${result.errors.join("; ")}`);
  return result;
}

export async function findAllVisibleAssets(
  application: ApplicationHarness,
  assetPath: string,
): Promise<Asset[]> {
  const expectedPath = canonicalizeWindowsPath(assetPath);
  const assets = await queryVisibleAssets(application);
  return assets.filter((asset) => canonicalizeWindowsPath(asset.path) === expectedPath);
}

export async function findVisibleAsset(
  application: ApplicationHarness,
  assetPath: string,
): Promise<Asset | undefined> {
  const matches = await findAllVisibleAssets(application, assetPath);
  assert.ok(matches.length <= 1, `同一路径出现 ${matches.length} 条可见资产：${assetPath}`);
  return matches[0];
}

export async function waitForVisibleAsset(
  application: ApplicationHarness,
  assetPath: string,
  description = `资产可见：${assetPath}`,
): Promise<Asset> {
  return waitUntil(
    description,
    async () => await findVisibleAsset(application, assetPath),
    { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
  );
}

export async function waitForVisibleAssetToDisappear(
  application: ApplicationHarness,
  assetPath: string,
  description = `资产从可见图库消失：${assetPath}`,
): Promise<void> {
  await waitUntil(
    description,
    async () => {
      const asset = await findVisibleAsset(application, assetPath);
      return asset === undefined ? true : undefined;
    },
    { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
  );
}

export async function getMissingAssets(
  application: ApplicationHarness,
): Promise<MissingAssetsResponse> {
  return application.call<MissingAssetsResponse>("gallery.getMissingAssets", {});
}

export function findMissingAsset(
  response: MissingAssetsResponse,
  assetPath: string,
): MissingAsset | undefined {
  const expectedPath = canonicalizeWindowsPath(assetPath);
  return response.items.find((asset) => canonicalizeWindowsPath(asset.path) === expectedPath);
}

export async function waitForMissingAsset(
  application: ApplicationHarness,
  assetPath: string,
  description = `资产进入 Missing：${assetPath}`,
): Promise<MissingAsset> {
  return waitUntil(
    description,
    async () => findMissingAsset(await getMissingAssets(application), assetPath),
    { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
  );
}

export async function waitForMissingAssetToDisappear(
  application: ApplicationHarness,
  assetPath: string,
  description = `资产离开 Missing：${assetPath}`,
): Promise<void> {
  await waitUntil(
    description,
    async () => {
      const missing = findMissingAsset(await getMissingAssets(application), assetPath);
      return missing === undefined ? true : undefined;
    },
    { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
  );
}

export function assertOperationSuccess(result: OperationResult, action: string): void {
  assert.equal(result.success, true, `${action}失败：${result.message}`);
}

export async function getFolderTree(
  application: ApplicationHarness,
): Promise<FolderTreeNode[]> {
  return application.call<FolderTreeNode[]>("gallery.getFolderTree", {});
}

export function findFolderByPath(
  tree: FolderTreeNode[],
  folderPath: string,
): FolderTreeNode | undefined {
  const expectedPath = canonicalizeWindowsPath(folderPath);
  const visit = (nodes: FolderTreeNode[]): FolderTreeNode | undefined => {
    for (const node of nodes) {
      if (canonicalizeWindowsPath(node.path) === expectedPath) {
        return node;
      }
      const child = visit(node.children ?? []);
      if (child) {
        return child;
      }
    }
    return undefined;
  };
  return visit(tree);
}

export async function createTag(
  application: ApplicationHarness,
  name: string,
): Promise<number> {
  return application.call<number>("gallery.createTag", { name });
}

export async function getAssetTags(
  application: ApplicationHarness,
  assetId: number,
): Promise<Tag[]> {
  return application.call<Tag[]>("gallery.getAssetTags", { assetId });
}

export async function captureGallerySnapshot(
  application: ApplicationHarness,
): Promise<GallerySnapshot> {
  const [visibleAssets, missingResponse] = await Promise.all([
    queryVisibleAssets(application),
    getMissingAssets(application),
  ]);

  const visible = visibleAssets
    .map((asset) => ({
      id: asset.id,
      path: canonicalizeWindowsPath(asset.path),
      hash: asset.hash,
      description: asset.description,
      rating: asset.rating,
      reviewFlag: asset.reviewFlag,
      fileModifiedAt: asset.fileModifiedAt,
    }))
    .sort((left, right) => left.path.localeCompare(right.path));

  const missing = missingResponse.items
    .map((asset) => ({
      id: asset.id,
      path: canonicalizeWindowsPath(asset.path),
      missingAt: asset.missingAt,
    }))
    .sort((left, right) => left.path.localeCompare(right.path));

  return { visible, missing };
}

export function assertGallerySnapshotEqual(
  actual: GallerySnapshot,
  expected: GallerySnapshot,
  message: string,
): void {
  assert.deepEqual(actual, expected, message);
}

export async function readJsonFile<T>(path: string): Promise<T> {
  return JSON.parse(await readFile(path, "utf8")) as T;
}
