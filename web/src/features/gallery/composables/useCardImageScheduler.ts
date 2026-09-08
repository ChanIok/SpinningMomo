import { onBeforeUnmount, ref, watch, type Ref } from 'vue'
import { yieldToBrowser } from './browserTaskScheduler'
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
  let isDestroyed = false

  // 持续消费者运行状态
  let isThumbnailConsumerRunning = false
  let isOriginalConsumerRunning = false
  let originalConsumerGeneration = 0

  // 完全清空原图许可，用于关闭原图模式或销毁调度器。
  function clearOriginalPermissions() {
    allowedOriginalAssetIds.value = new Set()
  }

  // 移除已经离开当前虚拟窗口的缩略图许可，避免调度器变成长期图片缓存。
  function pruneThumbnailPermissions() {
    const latestAssetIds = new Set(latestItems.map((item) => item.assetId))
    const currentAllowedIds = allowedThumbnailAssetIds.value
    const nextAllowedIds = new Set<number>()

    for (const assetId of currentAllowedIds) {
      if (latestAssetIds.has(assetId)) {
        nextAllowedIds.add(assetId)
      }
    }

    // 若无任何 ID 被修剪，避免给响应式变量赋新 Set 实例，防止父组件模板全量重算
    if (nextAllowedIds.size === currentAllowedIds.size) {
      return
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

  // 唤醒缩略图后台平滑预热消费者（已在运行时自动复用，不重复创建）
  function triggerThumbnailConsumer() {
    if (isThumbnailConsumerRunning || isDestroyed) {
      return
    }

    isThumbnailConsumerRunning = true
    void runThumbnailConsumerLoop()
  }

  // 持续消费循环：面向最新滚动位置，按距离视口中心由近及远分批放行
  async function runThumbnailConsumerLoop() {
    while (!isDestroyed) {
      // 1. 从当前最新的候选项中过滤出落在预热范围内且未授权的卡片
      const pendingItems = getPendingThumbnailItems(THUMBNAIL_PRELOAD_VIEWPORT_RATIO)
      if (pendingItems.length === 0) {
        break
      }

      // 2. 按离当前视口中心的距离升序排序：即将进入视口的卡片优先预热
      pendingItems.sort(
        (left, right) =>
          getItemDistanceToViewportCenter(left) - getItemDistanceToViewportCenter(right)
      )

      // 3. 取出本批次（Batch 6）
      const batch = pendingItems.slice(0, THUMBNAIL_BATCH_SIZE)

      // 4. 再次确认落在当前预热范围内，批量授权
      const currentAllowedIds = allowedThumbnailAssetIds.value
      const nextAllowedIds = new Set(currentAllowedIds)
      let hasNewAllowed = false

      for (const item of batch) {
        if (isItemInViewport(item, THUMBNAIL_PRELOAD_VIEWPORT_RATIO)) {
          nextAllowedIds.add(item.assetId)
          hasNewAllowed = true
        }
      }

      if (hasNewAllowed) {
        allowedThumbnailAssetIds.value = nextAllowedIds
      }

      // 5. 主动让出主线程，让浏览器处理手势输入与刷帧；醒来后自然读取最新视口
      await yieldToBrowser()
    }

    isThumbnailConsumerRunning = false
  }

  // 暂停原图升级消费者
  function pauseOriginalConsumer() {
    originalConsumerGeneration += 1
    isOriginalConsumerRunning = false
  }

  // 唤醒原图后台消费者
  function triggerOriginalConsumer() {
    if (isOriginalConsumerRunning || !originalEnabled.value || !isScrollIdle.value || isDestroyed) {
      return
    }

    isOriginalConsumerRunning = true
    void runOriginalConsumerLoop()
  }

  // 原图静止消费循环：在滚动完全停止后，逐张为视口中心卡片授予原图许可
  async function runOriginalConsumerLoop() {
    const generation = originalConsumerGeneration

    while (!isDestroyed && isScrollIdle.value && originalEnabled.value) {
      if (generation !== originalConsumerGeneration) {
        break
      }

      pruneOriginalPermissions()
      const pendingItems = prioritizeOriginalItems(getPendingOriginalItems())
      if (pendingItems.length === 0) {
        break
      }

      const targetItem = pendingItems[0]
      if (!targetItem || !isItemInViewport(targetItem, getOriginalPreloadViewportRatio())) {
        break
      }

      // 原图每次只新增一个许可，真实解码压力由 AssetCard 和 Worker 队列约束
      const nextAllowedIds = new Set(allowedOriginalAssetIds.value)
      nextAllowedIds.add(targetItem.assetId)
      allowedOriginalAssetIds.value = nextAllowedIds

      // 原图升级没有首屏刚需，逐项让出调度机会
      await yieldToBrowser()
    }

    if (generation === originalConsumerGeneration) {
      isOriginalConsumerRunning = false
    }
  }

  // 记录一次滚动输入：缩略图预热平稳跟进，原图暂停升级并在空闲后恢复
  function markScrolling() {
    isScrollIdle.value = false
    pauseOriginalConsumer()
    pruneOriginalPermissions()

    // 唤醒缩略图持续消费者平稳推进（已在运行则自动在下一 tick 消费最新位置）
    triggerThumbnailConsumer()

    // 连续滚动时刷新空闲窗口
    if (scrollIdleTimer !== null) {
      window.clearTimeout(scrollIdleTimer)
    }

    scrollIdleTimer = window.setTimeout(() => {
      isScrollIdle.value = true
      scrollIdleTimer = null

      // 空闲窗口恢复原图增强
      triggerOriginalConsumer()
    }, CARD_IMAGE_LOAD_IDLE_MS)
  }

  // 更新虚拟列表候选项，并按当前滚动状态启动对应调度
  function scheduleVisibleItems(items: CardImageScheduleItem[]) {
    latestItems = items
    pruneThumbnailPermissions()
    pruneOriginalPermissions()

    // 视口内严格可见的卡片（Ratio = 0）：VIP 绿色通道，立即同步授权！
    const currentAllowedIds = allowedThumbnailAssetIds.value
    const nextAllowedIds = new Set(currentAllowedIds)
    let hasNewVisible = false

    for (const item of items) {
      if (isItemInViewport(item, 0) && !nextAllowedIds.has(item.assetId)) {
        nextAllowedIds.add(item.assetId)
        hasNewVisible = true
      }
    }

    if (hasNewVisible) {
      allowedThumbnailAssetIds.value = nextAllowedIds
    }

    // 唤醒后台半屏平滑预热消费者
    triggerThumbnailConsumer()

    if (isScrollIdle.value) {
      triggerOriginalConsumer()
    }
  }

  // 查询卡片是否已经获得本轮缩略图加载许可
  function isThumbnailLoadAllowed(assetId: number): boolean {
    return allowedThumbnailAssetIds.value.has(assetId)
  }

  // 查询卡片是否已经获得本轮原图加载许可
  function isOriginalLoadAllowed(assetId: number): boolean {
    if (!originalEnabled.value) {
      return false
    }

    return allowedOriginalAssetIds.value.has(assetId)
  }

  watch(
    originalEnabled,
    (isEnabled) => {
      pauseOriginalConsumer()
      clearOriginalPermissions()

      if (!isEnabled) {
        return
      }

      if (isScrollIdle.value) {
        triggerOriginalConsumer()
      }
    },
    { immediate: true }
  )

  watch(isCompactWindow, () => {
    pauseOriginalConsumer()
    pruneOriginalPermissions()

    if (originalEnabled.value && isScrollIdle.value) {
      triggerOriginalConsumer()
    }
  })

  onBeforeUnmount(() => {
    isDestroyed = true

    if (scrollIdleTimer !== null) {
      window.clearTimeout(scrollIdleTimer)
    }

    pauseOriginalConsumer()
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
