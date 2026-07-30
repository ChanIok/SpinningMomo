import { onBeforeUnmount, ref, watch, type Ref } from 'vue'

const ORIGINAL_CARD_LOAD_IDLE_MS = 200
const MIN_ORIGINAL_CARD_SHORT_EDGE_PX = 360
const ORIGINAL_CARD_PRELOAD_VIEWPORT_RATIO = 0.5

type SchedulerPriority = 'user-blocking' | 'user-visible' | 'background'

interface BrowserScheduler {
  postTask?: <T>(
    callback: () => T | Promise<T>,
    options?: { priority?: SchedulerPriority; signal?: AbortSignal }
  ) => Promise<T>
  yield?: () => Promise<void>
}

interface TaskControllerLike {
  signal: AbortSignal
  abort: () => void
}

interface TaskControllerConstructor {
  new (options?: { priority?: SchedulerPriority }): TaskControllerLike
}

export interface OriginalCardScheduleItem {
  assetId: number
  start: number
  size: number
  width: number
  height: number
}

// 读取浏览器调度器；旧 WebView2 没有这些 API 时回退到 rAF。
function getBrowserScheduler(): BrowserScheduler | null {
  const globalWithScheduler = globalThis as typeof globalThis & {
    scheduler?: BrowserScheduler
  }

  return globalWithScheduler.scheduler ?? null
}

// 创建可取消的后台任务控制器，优先使用 Chromium 的 TaskController。
function createBackgroundTaskController(): TaskControllerLike {
  const globalWithTaskController = globalThis as typeof globalThis & {
    TaskController?: TaskControllerConstructor
  }

  if (globalWithTaskController.TaskController) {
    return new globalWithTaskController.TaskController({ priority: 'background' })
  }

  return new AbortController()
}

// 投递一个低优先级任务，避免原图许可在滚动热路径里同步派发。
async function postBackgroundTask(signal: AbortSignal, callback: () => void): Promise<void> {
  if (signal.aborted) {
    return
  }

  const scheduler = getBrowserScheduler()
  if (scheduler?.postTask) {
    await scheduler.postTask(callback, { priority: 'background', signal })
    return
  }

  await new Promise<void>((resolve) => {
    window.requestAnimationFrame(() => {
      if (!signal.aborted) {
        callback()
      }

      resolve()
    })
  })
}

// 让出一次浏览器调度机会，给输入处理和提交帧留出窗口。
async function yieldToBrowser(signal: AbortSignal): Promise<void> {
  if (signal.aborted) {
    return
  }

  const scheduler = getBrowserScheduler()
  if (scheduler?.yield) {
    await scheduler.yield()
    return
  }

  await new Promise<void>((resolve) => {
    window.requestAnimationFrame(() => resolve())
  })
}

// 控制原图卡片在滚动空闲后低优先级、逐项获得加载许可。
export function useOriginalCardScheduler(
  containerRef: Ref<HTMLElement | null>,
  enabled: Ref<boolean>
) {
  const isScrollIdle = ref(true)
  const allowedOriginalAssetIds = ref<Set<number>>(new Set())

  let latestItems: OriginalCardScheduleItem[] = []
  let scrollIdleTimer: ReturnType<typeof window.setTimeout> | null = null
  let scheduleVersion = 0
  let taskController = createBackgroundTaskController()

  // 中止未派发的原图许可，并让当前许可集合失效。
  function cancelScheduledLoads() {
    scheduleVersion += 1
    taskController.abort()
    taskController = createBackgroundTaskController()
    allowedOriginalAssetIds.value = new Set()
  }

  // 判断虚拟项是否进入原图预热区域；滚动空闲后提前半屏准备高清预览。
  function isItemVisible(item: OriginalCardScheduleItem): boolean {
    const container = containerRef.value
    if (!container) {
      return false
    }

    // 上下各放宽半个视口，让即将进入画面的卡片能在空闲期提前升级。
    const preloadMargin = container.clientHeight * ORIGINAL_CARD_PRELOAD_VIEWPORT_RATIO
    const viewportStart = container.scrollTop - preloadMargin
    const viewportEnd = container.scrollTop + container.clientHeight + preloadMargin
    const itemEnd = item.start + item.size
    return item.start < viewportEnd && itemEnd > viewportStart
  }

  // 根据实际屏幕像素判断小卡片是否值得升级原图。
  function isItemWorthOriginalLoad(item: OriginalCardScheduleItem): boolean {
    const shortEdge = Math.min(item.width, item.height)
    if (shortEdge <= 0) {
      return false
    }

    // CSS 尺寸乘以 DPR 后低于阈值时，480 短边缩略图已经足够承担卡片显示。
    return shortEdge * window.devicePixelRatio >= MIN_ORIGINAL_CARD_SHORT_EDGE_PX
  }

  // 取得当前预热区域内的项，并按 assetId 去重。
  function getVisibleItems(): OriginalCardScheduleItem[] {
    const seenAssetIds = new Set<number>()
    const visibleItems: OriginalCardScheduleItem[] = []

    for (const item of latestItems) {
      if (
        seenAssetIds.has(item.assetId) ||
        !isItemVisible(item) ||
        !isItemWorthOriginalLoad(item)
      ) {
        continue
      }

      seenAssetIds.add(item.assetId)
      visibleItems.push(item)
    }

    return visibleItems
  }

  // 启动一轮低优先级派发：逐个授予当前可见卡片原图加载许可。
  async function runSchedule() {
    if (!enabled.value || !isScrollIdle.value) {
      return
    }

    cancelScheduledLoads()

    const runVersion = scheduleVersion
    const signal = taskController.signal
    const visibleItems = getVisibleItems()

    for (const item of visibleItems) {
      if (signal.aborted || runVersion !== scheduleVersion || !isItemVisible(item)) {
        return
      }

      try {
        await postBackgroundTask(signal, () => {
          if (signal.aborted || runVersion !== scheduleVersion || !isItemVisible(item)) {
            return
          }

          // 每次只新增一个许可，让 AssetCard 的本地并发槽继续控制真实图片管线压力。
          const nextAllowedIds = new Set(allowedOriginalAssetIds.value)
          nextAllowedIds.add(item.assetId)
          allowedOriginalAssetIds.value = nextAllowedIds
        })

        await yieldToBrowser(signal)
      } catch (error) {
        if (!signal.aborted) {
          throw error
        }

        return
      }
    }
  }

  // 记录一次滚动输入：立即停止派发，并在空闲窗口后调度当前可见项。
  function markScrolling() {
    if (!enabled.value) {
      return
    }

    isScrollIdle.value = false
    cancelScheduledLoads()

    // 连续滚动时刷新空闲窗口，避免滚动中途启动原图解码。
    if (scrollIdleTimer !== null) {
      window.clearTimeout(scrollIdleTimer)
    }

    scrollIdleTimer = window.setTimeout(() => {
      isScrollIdle.value = true
      scrollIdleTimer = null
      void runSchedule()
    }, ORIGINAL_CARD_LOAD_IDLE_MS)
  }

  // 更新虚拟列表项；空闲时立即按最新可见区域重新派发。
  function scheduleVisibleItems(items: OriginalCardScheduleItem[]) {
    latestItems = items

    if (!enabled.value || !isScrollIdle.value) {
      return
    }

    void runSchedule()
  }

  // 查询卡片是否已经获得本轮原图加载许可。
  function isOriginalLoadAllowed(assetId: number): boolean {
    if (!enabled.value) {
      return false
    }

    return allowedOriginalAssetIds.value.has(assetId)
  }

  watch(
    enabled,
    (isEnabled) => {
      cancelScheduledLoads()

      if (!isEnabled) {
        isScrollIdle.value = true

        if (scrollIdleTimer !== null) {
          window.clearTimeout(scrollIdleTimer)
          scrollIdleTimer = null
        }

        return
      }

      // 重新开启原图模式时，按当前虚拟候选项重新派发许可。
      void runSchedule()
    },
    { immediate: true }
  )

  onBeforeUnmount(() => {
    if (scrollIdleTimer !== null) {
      window.clearTimeout(scrollIdleTimer)
    }

    cancelScheduledLoads()
  })

  return {
    isScrollIdle,
    markScrolling,
    scheduleVisibleItems,
    isOriginalLoadAllowed,
  }
}
