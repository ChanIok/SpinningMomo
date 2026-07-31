import { spawn, type ChildProcess } from "node:child_process";
import {
  access,
  cp,
  mkdir,
  mkdtemp,
  readFile,
  rm,
  writeFile,
} from "node:fs/promises";
import { createConnection } from "node:net";
import { tmpdir } from "node:os";
import { basename, dirname, join, relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";

const SCENARIO_DIRECTORY = dirname(fileURLToPath(import.meta.url));
export const REPOSITORY_ROOT = resolve(SCENARIO_DIRECTORY, "..", "..", "..");
// 场景测试是回归测试，默认验证发布给用户的实际构建（Release）。
export const DEFAULT_EXECUTABLE_PATH = join(
  REPOSITORY_ROOT,
  "build",
  "windows",
  "x64",
  "release",
  "SpinningMomo.exe",
);
export const DEFAULT_SCENARIO_WINDOW_EXECUTABLE_PATH = join(
  REPOSITORY_ROOT,
  "build",
  "windows",
  "x64",
  "release",
  "SpinningMomoScenarioWindow.exe",
);

const RPC_URL = "http://127.0.0.1:51206/rpc";
const RPC_PORT = 51206;
const SANDBOX_PREFIX = "spinning-momo-scenario-";

type JsonRpcErrorPayload = {
  code: number;
  message: string;
  data?: string;
};

type JsonRpcResponse<T> = {
  jsonrpc?: string;
  result?: T;
  error?: JsonRpcErrorPayload;
  id?: number;
};

export type ScenarioEnvironment = {
  rootDirectory: string;
  appDirectory: string;
  galleryDirectory: string;
  captureDirectory: string;
  databasePath: string;
  settingsPath: string;
  thumbnailsDirectory: string;
  executablePath: string;
  logPath: string;
};

export type WaitOptions = {
  timeoutMs?: number;
  intervalMs?: number;
  retryErrors?: boolean;
};

// 表示一次 JSON-RPC 调用已经到达后端，但后端返回了业务或协议错误。
export class RpcCallError extends Error {
  readonly method: string;
  readonly code: number;
  readonly data?: string;

  constructor(method: string, error: JsonRpcErrorPayload) {
    super(`RPC ${method} failed (${error.code}): ${error.message}`);
    this.name = "RpcCallError";
    this.method = method;
    this.code = error.code;
    this.data = error.data;
  }
}

// 把 Windows 路径转换为适合测试比较的大小写无关规范形式。
export function canonicalizeWindowsPath(value: string): string {
  return resolve(value).replaceAll("\\", "/").toLocaleLowerCase("en-US");
}

// 模块作为入口脚本被直接执行（而非被套件导入）时返回 true，用于保留单场景直跑能力。
export function isMainModule(metaUrl: string): boolean {
  const entry = process.argv[1];
  return entry !== undefined && resolve(entry) === fileURLToPath(metaUrl);
}

// 从命令行或环境变量解析被测程序，未指定时使用默认 Release 输出。
export function resolveExecutablePath(arguments_: string[]): string {
  const inlineArgument = arguments_.find((argument) => argument.startsWith("--exe="));
  if (inlineArgument) {
    return resolve(inlineArgument.slice("--exe=".length));
  }

  const argumentIndex = arguments_.indexOf("--exe");
  if (argumentIndex >= 0) {
    const value = arguments_[argumentIndex + 1];
    if (!value || value.startsWith("--")) {
      throw new Error("--exe 后必须提供 SpinningMomo.exe 的路径");
    }
    return resolve(value);
  }

  if (process.env.SPINNING_MOMO_EXE) {
    return resolve(process.env.SPINNING_MOMO_EXE);
  }

  return DEFAULT_EXECUTABLE_PATH;
}

// 从命令行或环境变量解析场景专用的外部目标窗口程序。
export function resolveScenarioWindowExecutablePath(arguments_: string[]): string {
  const inlineArgument = arguments_.find((argument) => argument.startsWith("--target-exe="));
  if (inlineArgument) {
    return resolve(inlineArgument.slice("--target-exe=".length));
  }

  const argumentIndex = arguments_.indexOf("--target-exe");
  if (argumentIndex >= 0) {
    const value = arguments_[argumentIndex + 1];
    if (!value || value.startsWith("--")) {
      throw new Error("--target-exe 后必须提供 SpinningMomoScenarioWindow.exe 的路径");
    }
    return resolve(value);
  }

  if (process.env.SPINNING_MOMO_SCENARIO_WINDOW_EXE) {
    return resolve(process.env.SPINNING_MOMO_SCENARIO_WINDOW_EXE);
  }

  const applicationPath = resolveExecutablePath(arguments_);
  if (applicationPath !== DEFAULT_EXECUTABLE_PATH) {
    return join(dirname(applicationPath), "SpinningMomoScenarioWindow.exe");
  }

  return DEFAULT_SCENARIO_WINDOW_EXECUTABLE_PATH;
}

// 轮询最终状态而不是等待固定时长，使场景不依赖机器速度。
export async function waitUntil<T>(
  description: string,
  probe: () => Promise<T | undefined>,
  options: WaitOptions = {},
): Promise<T> {
  const timeoutMs = options.timeoutMs ?? 10_000;
  const intervalMs = options.intervalMs ?? 100;
  const retryErrors = options.retryErrors ?? false;
  const deadline = Date.now() + timeoutMs;

  let lastError: unknown;
  while (Date.now() < deadline) {
    try {
      const result = await probe();
      if (result !== undefined) {
        return result;
      }
    } catch (error) {
      if (!retryErrors) {
        throw error;
      }
      lastError = error;
    }

    await delay(intervalMs);
  }

  const errorDetail = lastError ? `；最后异常：${formatError(lastError)}` : "";
  throw new Error(`等待超时：${description}（${timeoutMs}ms）${errorDetail}`);
}

// 创建一次性便携版目录，隔离数据库、设置、缩略图和日志。
export async function createScenarioEnvironment(
  sourceExecutablePath: string,
): Promise<ScenarioEnvironment> {
  await access(sourceExecutablePath).catch(() => {
    throw new Error(
      `找不到被测程序：${sourceExecutablePath}\n请先自行构建，场景脚本不会自动运行 Xmake。`,
    );
  });

  if (basename(sourceExecutablePath).toLocaleLowerCase("en-US") !== "spinningmomo.exe") {
    throw new Error(`--exe 必须指向 SpinningMomo.exe：${sourceExecutablePath}`);
  }

  if (await isTcpPortOpen(RPC_PORT)) {
    throw new Error(
      `127.0.0.1:${RPC_PORT} 已被占用。请先关闭正在运行的 SpinningMomo，再执行场景测试。`,
    );
  }

  const rootDirectory = await mkdtemp(join(tmpdir(), SANDBOX_PREFIX));
  const appDirectory = join(rootDirectory, "app");
  const galleryDirectory = join(rootDirectory, "gallery");
  const captureDirectory = join(rootDirectory, "capture-output");
  const dataDirectory = join(appDirectory, "data");
  const sourceDirectory = dirname(sourceExecutablePath);

  try {
    // 复制整个目标输出目录，保留可执行文件可能依赖的同目录运行时文件。
    await cp(sourceDirectory, appDirectory, {
      recursive: true,
      filter: (source) => {
        const relativePath = relative(sourceDirectory, source);
        if (!relativePath) {
          return true;
        }

        const firstSegment = relativePath.split(sep)[0]?.toLocaleLowerCase("en-US");
        return firstSegment !== "data" && firstSegment !== "portable";
      },
    });

    await mkdir(dataDirectory, { recursive: true });
    await mkdir(galleryDirectory, { recursive: true });
    await mkdir(captureDirectory, { recursive: true });
    await writeFile(join(appDirectory, "portable"), "");

    const settingsPath = join(dataDirectory, "settings.json");
    // 禁止测试实例申请管理员权限，也避免打开首次引导窗口。
    await writeFile(
      settingsPath,
      JSON.stringify(
        {
          app: {
            always_run_as_admin: false,
            onboarding: {
              completed: true,
            },
          },
        },
        undefined,
        2,
      ),
    );

    return {
      rootDirectory,
      appDirectory,
      galleryDirectory,
      captureDirectory,
      databasePath: join(dataDirectory, "database.db"),
      settingsPath,
      thumbnailsDirectory: join(dataDirectory, "thumbnails"),
      executablePath: join(appDirectory, basename(sourceExecutablePath)),
      logPath: join(dataDirectory, "logs", "app.log"),
    };
  } catch (error) {
    throw new Error(
      `创建场景环境失败：${formatError(error)}\n未完成的现场已保留：${rootDirectory}`,
    );
  }
}

// 管理场景专用的外部目标窗口；截图和录制的生产代码会排除主程序自身窗口。
export class TargetWindowHarness {
  readonly environment: ScenarioEnvironment;
  readonly executablePath: string;
  readonly title: string;
  readonly readyPath: string;
  readonly clientWidth: number;
  readonly clientHeight: number;
  private child: ChildProcess | undefined;
  private spawnError: Error | undefined;

  constructor(
    environment: ScenarioEnvironment,
    options: { clientWidth?: number; clientHeight?: number } = {},
  ) {
    const arguments_ = process.argv.slice(2);
    this.environment = environment;
    this.executablePath = resolveScenarioWindowExecutablePath(arguments_);
    this.title = `SpinningMomoScenarioTarget-${process.pid}-${Date.now()}`;
    this.readyPath = join(environment.rootDirectory, "target-window.ready");
    this.clientWidth = options.clientWidth ?? 640;
    this.clientHeight = options.clientHeight ?? 360;
  }

  async start(): Promise<void> {
    if (this.child) {
      throw new Error("场景目标窗口已经启动");
    }

    await access(this.executablePath).catch(() => {
      throw new Error(
        `找不到场景目标窗口程序：${this.executablePath}\n` +
          "请先自行构建 SpinningMomoScenarioWindow，场景脚本不会自动运行 Xmake。",
      );
    });

    await rm(this.readyPath, { force: true });
    this.child = spawn(
      this.executablePath,
      [
        "--title",
        this.title,
        "--width",
        String(this.clientWidth),
        "--height",
        String(this.clientHeight),
        "--ready",
        this.readyPath,
      ],
      {
        cwd: dirname(this.executablePath),
        stdio: "ignore",
        // 生产代码通过 IsWindowVisible 枚举目标窗口，不能让 GUI 窗口保持隐藏。
        windowsHide: false,
      },
    );
    this.child.once("error", (error) => {
      this.spawnError = error;
    });

    await waitUntil(
      "场景目标窗口完成创建",
      async () => {
        this.throwIfExited();
        try {
          await access(this.readyPath);
          return true;
        } catch (error) {
          if ((error as NodeJS.ErrnoException).code === "ENOENT") {
            return undefined;
          }
          throw error;
        }
      },
      { timeoutMs: 10_000, intervalMs: 100 },
    );
  }

  async stop(): Promise<void> {
    const child = this.child;
    this.child = undefined;

    if (child && !hasProcessExited(child)) {
      child.kill();
      if (!(await waitForProcessExit(child, 5_000))) {
        throw new Error(`无法终止场景目标窗口，PID=${child.pid ?? "unknown"}`);
      }
    }

    await rm(this.readyPath, { force: true });
  }

  private throwIfExited(): void {
    if (this.spawnError) {
      throw new Error(`无法启动场景目标窗口：${this.spawnError.message}`);
    }
    if (this.child && hasProcessExited(this.child)) {
      const exitReason =
        this.child.exitCode !== null
          ? `退出码 ${this.child.exitCode}`
          : `信号 ${this.child.signalCode ?? "unknown"}`;
      throw new Error(`场景目标窗口在就绪前退出：${exitReason}`);
    }
  }
}

// 仅清理本次运行创建且仍位于系统临时目录下的沙箱。
export async function removeScenarioEnvironment(environment: ScenarioEnvironment): Promise<void> {
  const resolvedRoot = resolve(environment.rootDirectory);
  const expectedParent = resolve(tmpdir());
  const isDirectChild = dirname(resolvedRoot) === expectedParent;
  const hasExpectedPrefix = basename(resolvedRoot).startsWith(SANDBOX_PREFIX);

  if (!isDirectChild || !hasExpectedPrefix) {
    throw new Error(`拒绝清理无法确认的场景目录：${resolvedRoot}`);
  }

  await rm(resolvedRoot, { recursive: true, force: true });
}

// 管理真实主程序的生命周期，并通过生产 HTTP 入口调用 JSON-RPC。
export class ApplicationHarness {
  readonly environment: ScenarioEnvironment;
  private child: ChildProcess | undefined;
  private spawnError: Error | undefined;
  private nextRequestId = 1;

  constructor(environment: ScenarioEnvironment) {
    this.environment = environment;
  }

  // 启动真实主程序，并等待主流程与 Gallery 后台初始化都完成。
  async start(): Promise<void> {
    if (this.child) {
      throw new Error("测试程序已经启动");
    }

    this.child = spawn(this.environment.executablePath, [], {
      cwd: this.environment.appDirectory,
      stdio: "ignore",
      windowsHide: false,
    });
    this.child.once("error", (error) => {
      this.spawnError = error;
    });

    await waitUntil(
      "SpinningMomo 与 Gallery 完成启动",
      async () => {
        this.throwIfExitedDuringStartup();

        const log = await readFile(this.environment.logPath, "utf8").catch(
          (error: NodeJS.ErrnoException) => {
            if (error.code === "ENOENT") {
              return "";
            }
            throw error;
          },
        );

        const appReady = log.includes("SpinningMomo startup ready");
        const galleryReady = log.includes("Gallery startup initialization completed");
        return appReady && galleryReady ? true : undefined;
      },
      { timeoutMs: 20_000, intervalMs: 100 },
    );

    // 日志就绪后再确认生产 RPC 入口已经能够访问数据库。
    let lastRpcError: unknown;
    await waitUntil(
      "Gallery RPC 可查询",
      async () => {
        this.throwIfExitedDuringStartup();
        try {
          await this.call("gallery.queryAssets", { filters: {} }, 1_000);
          return true;
        } catch (error) {
          lastRpcError = error;
          return undefined;
        }
      },
      { timeoutMs: 5_000, intervalMs: 100 },
    ).catch((error) => {
      throw new Error(
        `${formatError(error)}\n最后一次 RPC 错误：${formatError(lastRpcError)}`,
      );
    });
  }

  // 重启应用进程，用于测试配置持久化和数据库启动恢复逻辑。
  async restart(): Promise<void> {
    await this.stop();
    this.child = undefined;
    this.spawnError = undefined;
    await this.start();
  }

  // 通过生产 /rpc 端点执行一次 JSON-RPC 2.0 请求。
  async call<T>(method: string, params: unknown = {}, timeoutMs = 10_000): Promise<T> {
    const requestId = this.nextRequestId++;
    const response = await fetch(RPC_URL, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        jsonrpc: "2.0",
        method,
        params,
        id: requestId,
      }),
      signal: AbortSignal.timeout(timeoutMs),
    });

    const responseText = await response.text();
    if (!response.ok) {
      throw new Error(`RPC ${method} HTTP ${response.status}: ${responseText}`);
    }

    let payload: JsonRpcResponse<T>;
    try {
      payload = JSON.parse(responseText) as JsonRpcResponse<T>;
    } catch {
      throw new Error(`RPC ${method} 返回了无效 JSON：${responseText}`);
    }

    if (payload.error) {
      throw new RpcCallError(method, payload.error);
    }
    if (!Object.hasOwn(payload, "result")) {
      throw new Error(`RPC ${method} 响应缺少 result：${responseText}`);
    }

    return payload.result as T;
  }

  // 请求应用走生产退出流程；超时后只终止本次启动的子进程。
  async stop(): Promise<void> {
    const child = this.child;
    if (!child || hasProcessExited(child)) {
      return;
    }

    try {
      await this.call("commands.invoke", { id: "app.exit" }, 2_000);
    } catch {
      // 应用可能在返回 RPC 结果前关闭 HTTP；后续以进程是否退出为准。
    }

    if (await waitForProcessExit(child, 10_000)) {
      return;
    }

    child.kill();
    if (!(await waitForProcessExit(child, 5_000))) {
      throw new Error(`无法终止测试子进程，PID=${child.pid ?? "unknown"}`);
    }
  }

  // 启动期间若进程提前退出，立即报告而不是继续等到超时。
  private throwIfExitedDuringStartup(): void {
    if (this.spawnError) {
      throw new Error(`无法启动 SpinningMomo：${this.spawnError.message}`);
    }
    if (this.child && hasProcessExited(this.child)) {
      const exitReason =
        this.child.exitCode !== null
          ? `退出码 ${this.child.exitCode}`
          : `信号 ${this.child.signalCode ?? "unknown"}`;
      throw new Error(`SpinningMomo 在启动完成前退出：${exitReason}`);
    }
  }
}

// 把未知异常转换为稳定、可读的失败信息。
export function formatError(error: unknown): string {
  if (error instanceof Error) {
    return error.stack ?? error.message;
  }
  return String(error);
}

// 延迟短暂时间，让异步状态有机会推进。
async function delay(milliseconds: number): Promise<void> {
  await new Promise<void>((resolveDelay) => {
    setTimeout(resolveDelay, milliseconds);
  });
}

// 在启动测试实例前探测固定 RPC 端口，避免误连用户的日常实例。
async function isTcpPortOpen(port: number): Promise<boolean> {
  return await new Promise<boolean>((resolveProbe) => {
    const socket = createConnection({ host: "127.0.0.1", port });
    let settled = false;

    const finish = (isOpen: boolean) => {
      if (settled) {
        return;
      }
      settled = true;
      socket.destroy();
      resolveProbe(isOpen);
    };

    socket.setTimeout(500);
    socket.once("connect", () => finish(true));
    socket.once("timeout", () => finish(false));
    socket.once("error", () => finish(false));
  });
}

// 等待指定子进程退出，避免按进程名误伤其他实例。
async function waitForProcessExit(child: ChildProcess, timeoutMs: number): Promise<boolean> {
  if (hasProcessExited(child)) {
    return true;
  }

  return await new Promise<boolean>((resolveExit) => {
    let settled = false;
    const timer = setTimeout(() => finish(false), timeoutMs);

    const finish = (exited: boolean) => {
      if (settled) {
        return;
      }
      settled = true;
      clearTimeout(timer);
      child.off("exit", onExit);
      resolveExit(exited);
    };
    const onExit = () => finish(true);

    child.once("exit", onExit);
    // 关闭“首次检查”和“注册监听”之间的竞态窗口。
    if (hasProcessExited(child)) {
      finish(true);
    }
  });
}

// 同时识别正常退出和被信号终止，避免错过已经发生的 exit 事件。
function hasProcessExited(child: ChildProcess): boolean {
  return child.exitCode !== null || child.signalCode !== null;
}
