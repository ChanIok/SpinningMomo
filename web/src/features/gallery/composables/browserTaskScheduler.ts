export type BrowserTaskPriority = 'user-blocking' | 'user-visible' | 'background'

interface BrowserScheduler {
  postTask?: <T>(
    callback: () => T | Promise<T>,
    options?: { priority?: BrowserTaskPriority; signal?: AbortSignal }
  ) => Promise<T>
  yield?: () => Promise<void>
}

export interface BrowserTaskController {
  signal: AbortSignal
  abort: () => void
}

interface TaskControllerConstructor {
  new (options?: { priority?: BrowserTaskPriority }): BrowserTaskController
}

// 读取 Chromium 调度器；旧 WebView2 没有这些 API 时由调用方走降级路径。
function getBrowserScheduler(): BrowserScheduler | null {
  const globalWithScheduler = globalThis as typeof globalThis & {
    scheduler?: BrowserScheduler
  }

  return globalWithScheduler.scheduler ?? null
}

// 创建指定优先级的可取消任务控制器。
export function createBrowserTaskController(priority: BrowserTaskPriority): BrowserTaskController {
  const globalWithTaskController = globalThis as typeof globalThis & {
    TaskController?: TaskControllerConstructor
  }

  // Chromium 原生 TaskController 能把优先级传给浏览器调度器。
  if (globalWithTaskController.TaskController) {
    return new globalWithTaskController.TaskController({ priority })
  }

  // 旧环境只需要 AbortSignal 语义，优先级由 postTask 降级路径自然忽略。
  return new AbortController()
}

// 投递一个带优先级的浏览器任务。
export async function postBrowserTask(
  priority: BrowserTaskPriority,
  signal: AbortSignal,
  callback: () => void
): Promise<void> {
  if (signal.aborted) {
    return
  }

  const scheduler = getBrowserScheduler()
  if (scheduler?.postTask) {
    await scheduler.postTask(callback, { priority, signal })
    return
  }

  // rAF 降级保证任务至少让出当前输入/布局机会。
  await new Promise<void>((resolve) => {
    window.requestAnimationFrame(() => {
      if (!signal.aborted) {
        callback()
      }

      resolve()
    })
  })
}

// 主动让出一次浏览器调度机会，避免批量状态更新压住输入。
export async function yieldToBrowser(signal: AbortSignal): Promise<void> {
  if (signal.aborted) {
    return
  }

  const scheduler = getBrowserScheduler()
  if (scheduler?.yield) {
    await scheduler.yield()
    return
  }

  // rAF 降级让下一批任务进入下一帧。
  await new Promise<void>((resolve) => {
    window.requestAnimationFrame(() => resolve())
  })
}
