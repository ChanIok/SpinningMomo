import assert from "node:assert/strict";
import { copyFile, mkdir, readdir, readFile, stat } from "node:fs/promises";
import { join } from "node:path";

import {
  ApplicationHarness,
  REPOSITORY_ROOT,
  canonicalizeWindowsPath,
  createScenarioEnvironment,
  formatError,
  removeScenarioEnvironment,
  resolveExecutablePath,
  TargetWindowHarness,
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
  fileCreatedAt?: number | null;
  createdAt?: number | null;
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

export type TagTreeNode = {
  id: number;
  name: string;
  parentId?: number | null;
  children?: TagTreeNode[];
};

export type TagStats = {
  tagId: number;
  tagName: string;
  assetCount: number;
};

export type HomeStats = {
  totalCount: number;
  photoCount: number;
  videoCount: number;
  livePhotoCount: number;
  totalSize: number;
  todayAddedCount: number;
};

export type BatchSelectionSummary = {
  selectedCount: number;
  rating?: number | null;
  rejectedState?: boolean | null;
  description?: string | null;
  commonTags: Tag[];
};

export type PurgeMissingAssetsResult = {
  success: boolean;
  message: string;
  deletedAssetCount: number;
  skippedAssetCount: number;
  deletedThumbnailCount: number;
  releasedThumbnailBytes: number;
  failedThumbnailCount: number;
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

export type TargetWindowScenarioAction = (
  application: ApplicationHarness,
  environment: ScenarioEnvironment,
  targetWindow: TargetWindowHarness,
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

// 在真实应用场景外再托管一个独立进程，提供截图/录制所需的稳定目标窗口。
export async function runScenarioWithTargetWindow(
  name: string,
  action: TargetWindowScenarioAction,
): Promise<void> {
  await runScenario(name, async (application, environment) => {
    const targetWindow = new TargetWindowHarness(environment);
    try {
      await targetWindow.start();
      await action(application, environment, targetWindow);
    } finally {
      await targetWindow.stop();
    }
  });
}

// 一个场景阶段：在共享生命周期内独立执行、独立报告，失败时以阶段名定位。
export type ScenarioPhase<C = void> = {
  name: string;
  action: (
    application: ApplicationHarness,
    environment: ScenarioEnvironment,
    context?: C,
  ) => Promise<void>;
};

// 在同一个真实进程与沙箱生命周期内按顺序执行多个相互独立的场景阶段，
// 减少重复的进程启动与沙箱复制开销；失败现场保留逻辑与 runScenario 一致。
// createContext 用于需要跨阶段共享额外资源（如捕获目标窗口）的场景。
export async function runScenarioPhases<C>(
  suiteName: string,
  phases: ScenarioPhase<C>[],
  createContext?: (
    application: ApplicationHarness,
    environment: ScenarioEnvironment,
  ) => Promise<{ context: C; dispose: () => Promise<void> }>,
): Promise<void> {
  await runScenario(suiteName, async (application, environment) => {
    const prepared = createContext
      ? await createContext(application, environment)
      : { context: undefined as C, dispose: async () => {} };
    try {
      for (const { name: phaseName, action } of phases) {
        console.log(`[${suiteName} :: ${phaseName}] 开始`);
        try {
          await action(application, environment, prepared.context);
          console.log(`PASS ${suiteName} :: ${phaseName}`);
        } catch (error) {
          throw new Error(`phase ${phaseName} 失败：\n${formatError(error)}`);
        }
      }
    } finally {
      await prepared.dispose();
    }
  });
}

export type InvokeCommandResult = {
  success: boolean;
  message: string;
};

// Command RPC 的 success 只代表 action 被派发；场景仍需等待实际文件结果。
export async function invokeCommand(
  application: ApplicationHarness,
  id: string,
): Promise<InvokeCommandResult> {
  const result = await application.call<InvokeCommandResult>("commands.invoke", { id });
  assert.equal(result.success, true, `调用 Command ${id} 失败：${result.message}`);
  return result;
}

export async function listDirectoryFileNames(directory: string): Promise<string[]> {
  const entries = await readdir(directory, { withFileTypes: true });
  return entries
    .filter((entry) => entry.isFile())
    .map((entry) => entry.name)
    .sort((left, right) => left.localeCompare(right));
}

export async function waitForNewFile(
  directory: string,
  existingNames: ReadonlySet<string>,
  predicate: (fileName: string) => boolean,
  description: string,
): Promise<string> {
  return waitUntil(
    description,
    async () => {
      const fileName = (await listDirectoryFileNames(directory)).find(
        (candidate) => !existingNames.has(candidate) && predicate(candidate),
      );
      return fileName ? join(directory, fileName) : undefined;
    },
    { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
  );
}

// 等待文件至少经历一次增长，避免把刚创建但还没有写入帧的录制工作文件当成就绪。
export async function waitForFileToGrow(path: string, description: string): Promise<number> {
  let previousSize: number | undefined;
  return waitUntil(
    description,
    async () => {
      const currentSize = (await stat(path)).size;
      const hasGrown = previousSize !== undefined && currentSize > previousSize;
      previousSize = currentSize;
      return hasGrown ? currentSize : undefined;
    },
    { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
  );
}

// 录制至少持续一段时间，并在等待期间确认编码器的工作文件仍然存在。
export async function waitForFileToRemainPresent(
  path: string,
  durationMs: number,
  description: string,
): Promise<number> {
  const deadline = Date.now() + durationMs;
  return waitUntil(
    description,
    async () => {
      const currentSize = (await stat(path)).size;
      return Date.now() >= deadline ? currentSize : undefined;
    },
    { timeoutMs: durationMs + 5_000, intervalMs: 100 },
  );
}

export async function waitForFileToStabilize(path: string, description: string): Promise<number> {
  let previousSize: number | undefined;
  let stablePolls = 0;
  return waitUntil(
    description,
    async () => {
      const currentSize = (await stat(path)).size;
      if (currentSize > 0 && currentSize === previousSize) {
        stablePolls++;
      } else {
        stablePolls = 0;
      }
      previousSize = currentSize;
      return stablePolls >= 2 ? currentSize : undefined;
    },
    { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
  );
}

export async function waitForPathToDisappear(path: string, description: string): Promise<void> {
  await waitUntil(
    description,
    async () => {
      try {
        await stat(path);
        return undefined;
      } catch (error) {
        if ((error as NodeJS.ErrnoException).code === "ENOENT") {
          return true;
        }
        throw error;
      }
    },
    { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
  );
}

export async function assertPngDimensions(
  path: string,
  expectedWidth: number,
  expectedHeight: number,
): Promise<void> {
  const data = await readFile(path);
  assert.ok(data.length >= 24, `PNG 文件过短：${path}`);
  assert.deepEqual(
    [...data.subarray(0, 8)],
    [137, 80, 78, 71, 13, 10, 26, 10],
    `PNG signature 无效：${path}`,
  );
  assert.equal(data.toString("ascii", 12, 16), "IHDR", `PNG 缺少 IHDR：${path}`);
  assert.equal(data.readUInt32BE(16), expectedWidth, `PNG 宽度不符：${path}`);
  assert.equal(data.readUInt32BE(20), expectedHeight, `PNG 高度不符：${path}`);
}

export async function assertMp4Structure(path: string): Promise<void> {
  const data = await readFile(path);
  const boxTypes = new Set<string>();
  let offset = 0;

  while (offset + 8 <= data.length) {
    let boxSize = data.readUInt32BE(offset);
    let headerSize = 8;
    if (boxSize === 1) {
      assert.ok(offset + 16 <= data.length, `MP4 large box header 不完整：${path}`);
      const largeSize = data.readBigUInt64BE(offset + 8);
      assert.ok(largeSize <= BigInt(Number.MAX_SAFE_INTEGER), `MP4 box 过大：${path}`);
      boxSize = Number(largeSize);
      headerSize = 16;
    } else if (boxSize === 0) {
      boxSize = data.length - offset;
    }

    assert.ok(boxSize >= headerSize && offset + boxSize <= data.length, `MP4 box 无效：${path}`);
    boxTypes.add(data.toString("ascii", offset + 4, offset + 8));
    offset += boxSize;
  }

  assert.equal(offset, data.length, `MP4 末尾存在无法解析的数据：${path}`);
  assert.ok(boxTypes.has("ftyp"), `MP4 缺少 ftyp box：${path}`);
  assert.ok(boxTypes.has("moov"), `MP4 缺少 moov box：${path}`);
}

// 每个 fixture 必须是内容唯一（hash 不同）的有效图片。
// 图库按内容 hash 共享继承与缩略图，依赖 hash 语义的场景必须使用独立 fixture。
const SCENARIO_FIXTURE_PATHS = {
  logo: join(REPOSITORY_ROOT, "web", "public", "logo_192x192.png"),
  blue: join(REPOSITORY_ROOT, "tests", "scenarios", "fixtures", "solid_blue.png"),
  green: join(REPOSITORY_ROOT, "tests", "scenarios", "fixtures", "solid_green.png"),
} as const;

export type ScenarioFixtureName = keyof typeof SCENARIO_FIXTURE_PATHS;

export function scenarioFixturePath(name: ScenarioFixtureName = "logo"): string {
  return SCENARIO_FIXTURE_PATHS[name];
}

export async function copyScenarioFixture(
  environment: ScenarioEnvironment,
  relativePath: string,
  sourceName: ScenarioFixtureName = "logo",
): Promise<string> {
  const destination = join(environment.galleryDirectory, relativePath);
  await mkdir(join(destination, ".."), { recursive: true });
  await copyFile(scenarioFixturePath(sourceName), destination);
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

export async function queryAssets(
  application: ApplicationHarness,
  filters: Record<string, unknown>,
  options: { sortBy?: string; sortOrder?: string; page?: number; perPage?: number } = {},
): Promise<QueryAssetsResponse> {
  return application.call<QueryAssetsResponse>("gallery.queryAssets", {
    filters,
    sortBy: options.sortBy,
    sortOrder: options.sortOrder,
    page: options.page,
    perPage: options.perPage,
  });
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

export function findTagInTree(
  tree: TagTreeNode[],
  tagId: number,
): TagTreeNode | undefined {
  for (const node of tree) {
    if (node.id === tagId) {
      return node;
    }
    const child = findTagInTree(node.children ?? [], tagId);
    if (child) {
      return child;
    }
  }
  return undefined;
}

export async function createTag(
  application: ApplicationHarness,
  name: string,
  parentId?: number,
): Promise<number> {
  return application.call<number>("gallery.createTag", { name, parentId });
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
