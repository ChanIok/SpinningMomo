import { onBeforeUnmount, ref, watch, type Ref } from 'vue'
import {
  createBrowserTaskController,
  postBrowserTask,
  yieldToBrowser,
} from './browserTaskScheduler'
import { MIN_ORIGINAL_CARD_SHORT_EDGE_PX } from '../constants'

const CARD_IMAGE_LOAD_IDLE_MS = 100
const THUMBNAIL_BATCH_SIZE = 6
const THUMBNAIL_PRELOAD_VIEWPORT_RATIO = 0.5
const ORIGINAL_CARD_PRELOAD_VIEWPORT_RATIO = 0.5
const COMPACT_ORIGINAL_CARD_PRELOAD_VIEWPORT_RATIO = 1

export interface CardImageScheduleItem {
  assetId: number
  start: number
  size: number
  width: number
  height: number
}

// 控制卡片缩略图与原图覆盖层的加载许可，避免滚动热路径一次启动大量图片管线。
export function useCardImageScheduler(
  containerRef: Ref<HTMLElement | null>,
  originalEnabled: Ref<boolean>,
  isCompactWindow: Ref<boolean>
) {
  const isScrollIdle = ref(true)
  const allowedThumbnailAssetIds = ref<Set<number>>(new Set())
  const allowedOriginalAssetIds = ref<Set<number>>(new Set())

  let latestItems: CardImageScheduleItem[] = []
  let scrollIdleTimer: number | null = null
  let thumbnailScheduleVersion = 0
  let originalScheduleVersion = 0
  let thumbnailTaskController = createBrowserTaskController('user-visible')
  let originalTaskController = createBrowserTaskController('background')

  // 中止未派发的缩略图任务，但保留当前虚拟窗口中已经获得许可的卡片。
  function cancelThumbnailSchedule() {
    thumbnailScheduleVersion += 1
    thumbnailTaskController.abort()
    thumbnailTaskController = createBrowserTaskController('user-visible')
  }

  // 中止未派发的原图调度任务；已有许可由调用方按当前预热范围决定是否保留。
  function cancelOriginalSchedule() {
    originalScheduleVersion += 1
    originalTaskController.abort()
    originalTaskController = createBrowserTaskController('background')
  }

  // 完全清空原图许可，用于关闭原图模式或销毁调度器。
  function clearOriginalPermissions() {
    allowedOriginalAssetIds.value = new Set()
  }

  // 移除已经离开当前虚拟窗口的缩略图许可，避免调度器变成长期图片缓存。
  function pruneThumbnailPermissions() {
    const latestAssetIds = new Set(latestItems.map((item) => item.assetId))
    const nextAllowedIds = new Set<number>()

    for (const assetId of allowedThumbnailAssetIds.value) {
      if (latestAssetIds.has(assetId)) {
        nextAllowedIds.add(assetId)
      }
    }

    allowedThumbnailAssetIds.value = nextAllowedIds
  }

  function getOriginalPreloadViewportRatio(): number {
    return isCompactWindow.value
      ? COMPACT_ORIGINAL_CARD_PRELOAD_VIEWPORT_RATIO
      : ORIGINAL_CARD_PRELOAD_VIEWPORT_RATIO
  }

  // 只保留当前原图预热范围内的许可，避免滚动时重复取消仍然相关的卡片。
  function pruneOriginalPermissions() {
    const container = containerRef.value
    if (!container || container.clientHeight <= 0) {
      return
    }

    const currentAllowedIds = allowedOriginalAssetIds.value
    const nextAllowedIds = new Set<number>()

    for (const item of latestItems) {
      if (
        currentAllowedIds.has(item.assetId) &&
        isItemInViewport(item, getOriginalPreloadViewportRatio())
      ) {
        nextAllowedIds.add(item.assetId)
      }
    }

    if (
      nextAllowedIds.size === currentAllowedIds.size &&
      Array.from(nextAllowedIds).every((assetId) => currentAllowedIds.has(assetId))
    ) {
      return
    }

    allowedOriginalAssetIds.value = nextAllowedIds
  }

  // 判断虚拟项是否落在指定预热范围内。
  function isItemInViewport(item: CardImageScheduleItem, preloadViewportRatio: number): boolean {
    const container = containerRef.value
    if (!container) {
      return false
    }

    // 预热范围按视口高度扩张，缩略图和原图都只消费虚拟窗口附近的候选项。
    const preloadMargin = container.clientHeight * preloadViewportRatio
    const viewportStart = container.scrollTop - preloadMargin
    const viewportEnd = container.scrollTop + container.clientHeight + preloadMargin
    const itemEnd = item.start + item.size
    return item.start < viewportEnd && itemEnd > viewportStart
  }

  // 判断卡片尺寸是否值得启动原图增强。
  function isItemWorthOriginalLoad(item: CardImageScheduleItem): boolean {
    const shortEdge = Math.min(item.width, item.height)
    if (shortEdge <= 0) {
      return false
    }

    // CSS 尺寸乘以 DPR 后低于阈值时，缩略图已经足够承担卡片显示。
    return shortEdge * window.devicePixelRatio >= MIN_ORIGINAL_CARD_SHORT_EDGE_PX
  }

  // 收集当前需要授予缩略图许可的卡片，并按 assetId 去重。
  function getPendingThumbnailItems(preloadViewportRatio: number): CardImageScheduleItem[] {
    const seenAssetIds = new Set<number>()
    const pendingItems: CardImageScheduleItem[] = []

    for (const item of latestItems) {
      if (
        seenAssetIds.has(item.assetId) ||
        allowedThumbnailAssetIds.value.has(item.assetId) ||
        !isItemInViewport(item, preloadViewportRatio)
      ) {
        continue
      }

      seenAssetIds.add(item.assetId)
      pendingItems.push(item)
    }

    return pendingItems
  }

  // 收集当前可升级原图的卡片，并按 assetId 去重。
  function getPendingOriginalItems(): CardImageScheduleItem[] {
    const seenAssetIds = new Set<number>()
    const pendingItems: CardImageScheduleItem[] = []

    for (const item of latestItems) {
      if (
        seenAssetIds.has(item.assetId) ||
        allowedOriginalAssetIds.value.has(item.assetId) ||
        !isItemInViewport(item, getOriginalPreloadViewportRatio()) ||
        !isItemWorthOriginalLoad(item)
      ) {
        continue
      }

      seenAssetIds.add(item.assetId)
      pendingItems.push(item)
    }

    return pendingItems
  }

  // 让当前视口内的卡片先进入原图队列，并按视口中心距离排序。
  function prioritizeOriginalItems(items: CardImageScheduleItem[]) {
    const visibleItems = items.filter((item) => isItemInViewport(item, 0))
    const nearbyItems = items.filter((item) => !isItemInViewport(item, 0))
    const compareDistanceToViewportCenter = (
      left: CardImageScheduleItem,
      right: CardImageScheduleItem
    ) => getItemDistanceToViewportCenter(left) - getItemDistanceToViewportCenter(right)

    visibleItems.sort(compareDistanceToViewportCenter)
    nearbyItems.sort(compareDistanceToViewportCenter)

    return [...visibleItems, ...nearbyItems]
  }

  function getItemDistanceToViewportCenter(item: CardImageScheduleItem): number {
    const container = containerRef.value
    if (!container) {
      return 0
    }

    const viewportCenter = container.scrollTop + container.clientHeight / 2
    const itemCenter = item.start + item.size / 2
    return Math.abs(itemCenter - viewportCenter)
  }

  // 分批授予半屏范围内的缩略图加载许可，让基础图片始终保持连续预热。
  async function runThumbnailSchedule() {
    cancelThumbnailSchedule()

    const runVersion = thumbnailScheduleVersion
    const signal = thumbnailTaskController.signal
    const pendingItems = getPendingThumbnailItems(THUMBNAIL_PRELOAD_VIEWPORT_RATIO)

    for (let index = 0; index < pendingItems.length; index += THUMBNAIL_BATCH_SIZE) {
      const batch = pendingItems.slice(index, index + THUMBNAIL_BATCH_SIZE)
      if (signal.aborted || runVersion !== thumbnailScheduleVersion) {
        return
      }

      try {
        await postBrowserTask('user-visible', signal, () => {
          if (signal.aborted || runVersion !== thumbnailScheduleVersion) {
            return
          }

          // 只给仍在当前预热范围内的卡片发许可，避免慢任务追上旧滚动位置。
          const nextAllowedIds = new Set(allowedThumbnailAssetIds.value)
          for (const item of batch) {
            if (isItemInViewport(item, THUMBNAIL_PRELOAD_VIEWPORT_RATIO)) {
              nextAllowedIds.add(item.assetId)
            }
          }
          allowedThumbnailAssetIds.value = nextAllowedIds
        })

        // 每一批后让浏览器先处理输入和提交帧。
        await yieldToBrowser(signal)
      } catch (error) {
        if (isAbortError(error)) {
          return
        }

        throw error
      }
    }
  }

  // 逐个授予原图加载许可；视口内卡片先排队，其余候选项继续后台推进。
  async function runOriginalSchedule() {
    if (!originalEnabled.value || !isScrollIdle.value) {
      return
    }

    cancelOriginalSchedule()
    pruneOriginalPermissions()

    const runVersion = originalScheduleVersion
    const signal = originalTaskController.signal
    const pendingItems = prioritizeOriginalItems(getPendingOriginalItems())

    for (const item of pendingItems) {
      if (
        signal.aborted ||
        runVersion !== originalScheduleVersion ||
        !isItemInViewport(item, getOriginalPreloadViewportRatio())
      ) {
        return
      }

      try {
        await postBrowserTask('background', signal, () => {
          if (
            signal.aborted ||
            runVersion !== originalScheduleVersion ||
            !isItemInViewport(item, getOriginalPreloadViewportRatio())
          ) {
            return
          }

          // 原图每次只新增一个许可，真实解码压力继续由 AssetCard 和 Worker 队列约束。
          const nextAllowedIds = new Set(allowedOriginalAssetIds.value)
          nextAllowedIds.add(item.assetId)
          allowedOriginalAssetIds.value = nextAllowedIds
        })

        // 原图升级没有首屏刚需，逐项让出调度机会。
        await yieldToBrowser(signal)
      } catch (error) {
        if (isAbortError(error)) {
          return
        }

        throw error
      }
    }
  }

  // 记录一次滚动输入：缩略图继续小批量推进，原图保留范围内许可并等空闲后补充新卡片。
  function markScrolling() {
    isScrollIdle.value = false
    cancelOriginalSchedule()
    pruneOriginalPermissions()

    // 滚动中也按同一策略推进半屏缩略图，响应性由小批次和 yield 保证。
    void runThumbnailSchedule()

    // 连续滚动时刷新空闲窗口，避免中途启动原图解码。
    if (scrollIdleTimer !== null) {
      window.clearTimeout(scrollIdleTimer)
    }

    scrollIdleTimer = window.setTimeout(() => {
      isScrollIdle.value = true
      scrollIdleTimer = null

      // 空闲窗口只恢复原图增强，缩略图始终由同一套调度持续推进。
      void runOriginalSchedule()
    }, CARD_IMAGE_LOAD_IDLE_MS)
  }

  // 更新虚拟列表候选项，并按当前滚动状态启动对应调度。
  function scheduleVisibleItems(items: CardImageScheduleItem[]) {
    latestItems = items
    pruneThumbnailPermissions()
    pruneOriginalPermissions()

    // 虚拟窗口变化后先保证半屏缩略图进入分批加载队列。
    void runThumbnailSchedule()

    if (isScrollIdle.value) {
      void runOriginalSchedule()
    }
  }

  // 查询卡片是否已经获得本轮缩略图加载许可。
  function isThumbnailLoadAllowed(assetId: number): boolean {
    return allowedThumbnailAssetIds.value.has(assetId)
  }

  // 查询卡片是否已经获得本轮原图加载许可。
  function isOriginalLoadAllowed(assetId: number): boolean {
    if (!originalEnabled.value) {
      return false
    }

    return allowedOriginalAssetIds.value.has(assetId)
  }

  watch(
    originalEnabled,
    (isEnabled) => {
      cancelOriginalSchedule()
      clearOriginalPermissions()

      if (!isEnabled) {
        return
      }

      // 重新开启原图模式时，按当前虚拟候选项重新派发增强层许可。
      void runOriginalSchedule()
    },
    { immediate: true }
  )

  watch(isCompactWindow, () => {
    cancelOriginalSchedule()
    pruneOriginalPermissions()

    if (originalEnabled.value && isScrollIdle.value) {
      void runOriginalSchedule()
    }
  })

  onBeforeUnmount(() => {
    if (scrollIdleTimer !== null) {
      window.clearTimeout(scrollIdleTimer)
    }

    cancelThumbnailSchedule()
    cancelOriginalSchedule()
    clearOriginalPermissions()
  })

  return {
    isScrollIdle,
    markScrolling,
    scheduleVisibleItems,
    isThumbnailLoadAllowed,
    isOriginalLoadAllowed,
  }
}

// 判断异步调度失败是否来自任务取消。
function isAbortError(error: unknown): boolean {
  return error instanceof DOMException && error.name === 'AbortError'
}
