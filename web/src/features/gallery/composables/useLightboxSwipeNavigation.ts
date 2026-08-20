import { computed, onUnmounted, ref, watch, type CSSProperties, type Ref } from 'vue'
import { useEventListener } from '@vueuse/core'
import { galleryApi } from '../api'
import { useGalleryData } from './useGalleryData'
import { useGalleryStore } from '../store'
import type { Asset } from '../types'

// 小于这个总位移时不锁定方向，避免轻触被误判为横向切图。
const SWIPE_AXIS_LOCK_THRESHOLD = 8
// 只用最近一小段轨迹计算速度，贴近手指松开前的真实速度。
const SWIPE_VELOCITY_WINDOW_MS = 100
// 慢拖按屏宽比例提交，同时设置上下限避免不同窗口下阈值失控。
const SWIPE_COMMIT_SLOW_DISTANCE_RATIO = 0.24
const SWIPE_COMMIT_SLOW_DISTANCE_MIN = 72
const SWIPE_COMMIT_SLOW_DISTANCE_MAX = 140
// 快扫只看方向速度，不要求先跨过较长距离。
const SWIPE_FLING_VELOCITY = 200
// 视觉动画保持固定时长；输入可以在动画期间中断它。
const SWIPE_SETTLE_DURATION = 400
// 所有触摸手势结束后的合成 click 都不能落到媒体或 chrome 上。
const TOUCH_GESTURE_CLICK_SUPPRESS_DURATION = 350
// 图片平移在水平边界外积累到这个距离后，才把同一次手势交给 Pager 切图。
const PAN_EDGE_HANDOFF_THRESHOLD = 12
// 图片平移开始时锁定主轴，避免竖向浏览时的轻微横向偏移误触切图。
const PAN_HORIZONTAL_AXIS_RATIO = 1.2
// 纵向手势同样需要明显的主轴优势，避免斜向拖动在切图和抽屉之间摇摆。
const VERTICAL_AXIS_RATIO = 1.2
// 纵向慢拖按视口高度比例提交，同时设置上下限避免小窗口阈值过低。
const VERTICAL_COMMIT_DISTANCE_RATIO = 0.18
const VERTICAL_COMMIT_DISTANCE_MIN = 72
const VERTICAL_COMMIT_DISTANCE_MAX = 160
// 纵向快速甩动沿用抽屉的触感：速度足够时不要求跨过完整距离。
const VERTICAL_FLING_VELOCITY = 700

// 触摸状态机不应在生产环境逐 pointer 事件写日志；需要诊断时再临时打开。
const LIGHTBOX_GESTURE_DEBUG = false

export type LightboxSwipeDirection = 'previous' | 'next'
export type LightboxSwipePhase = 'idle' | 'pending' | 'dragging' | 'settling'
export type LightboxVerticalGestureAction = 'dismiss' | 'details'

type ReadonlyNumberRef = Readonly<Ref<number>>
type ReadonlyBooleanRef = Readonly<Ref<boolean>>
type SwipeSample = { x: number; y: number; time: number }
type SwipePage = { index: number; asset: Asset }
type TouchPointer = Pick<PointerEvent, 'pointerId' | 'clientX' | 'clientY'>
type TouchPointerPair = [TouchPointer, TouchPointer]
type TouchGestureMode =
  'pending' | 'swiping' | 'panning' | 'vertical' | 'pinching' | 'pinch-complete'
type PanAxis = 'horizontal' | 'vertical'

interface PanMoveResult {
  residualX: number
  residualY: number
}

interface UseLightboxSwipeNavigationOptions {
  gestureSurfaceRef: Ref<HTMLElement | null>
  availableWidth: ReadonlyNumberRef
  availableHeight: ReadonlyNumberRef
  enabled: ReadonlyBooleanRef
  pannable: ReadonlyBooleanRef
  verticalGestureEnabled: ReadonlyBooleanRef
  navigateToIndex: (index: number) => void
  canStartGesture?: (event: PointerEvent) => boolean
  canStartVerticalGesture?: (target: EventTarget | null) => boolean
  onTouchTap?: (event: PointerEvent, startTarget: EventTarget | null) => boolean
  onPanStart?: (event: PointerEvent) => void
  onPanMove?: (event: PointerEvent) => PanMoveResult | void
  onPanEnd?: (event: PointerEvent) => void
  onPanCancel?: () => void
  onVerticalGestureMove?: (offsetY: number, progress: number) => void
  onVerticalGestureCancel?: (offsetY: number) => void
  onVerticalGestureCommit?: (action: LightboxVerticalGestureAction, offsetY: number) => void
  onPinchStart?: (pointers: TouchPointerPair) => void
  onPinchMove?: (pointers: TouchPointerPair) => void
  onPinchEnd?: () => void
}

// 只有能显示缩略图的媒体才进入 Pager 的相邻页面缓存。
function isNavigableAsset(asset: Asset | null | undefined): asset is Asset {
  return asset?.type === 'photo' || asset?.type === 'live_photo' || asset?.type === 'video'
}

/**
 * 灯箱媒体共用的触摸切图状态机。
 *
 * Pager 保留当前逻辑索引两侧的页面，手势只移动轨道，不直接改动媒体组件。
 * 这样动画进行中可以重新接管指针，并把新的手势接在当前轨道位置上。
 */
export function useLightboxSwipeNavigation(options: UseLightboxSwipeNavigationOptions) {
  const {
    gestureSurfaceRef,
    availableWidth,
    availableHeight,
    enabled,
    pannable,
    verticalGestureEnabled,
    navigateToIndex,
    canStartGesture,
    canStartVerticalGesture,
    onTouchTap,
    onPanStart,
    onPanMove,
    onPanEnd,
    onPanCancel,
    onVerticalGestureMove,
    onVerticalGestureCancel,
    onVerticalGestureCommit,
    onPinchStart,
    onPinchMove,
    onPinchEnd,
  } = options
  const store = useGalleryStore()
  const galleryData = useGalleryData()

  const baseIndex = ref<number | null>(null)
  const pageAssets = ref<SwipePage[]>([])
  const swipePointerId = ref<number | null>(null)
  const swipeStartX = ref(0)
  const swipeStartY = ref(0)
  const swipeStartOffset = ref(0)
  const swipeGestureOriginOffset = ref(0)
  const swipeGestureOriginIndex = ref<number | null>(null)
  const swipeDirection = ref<LightboxSwipeDirection | null>(null)
  const swipeOffset = ref(0)
  const swipePhase = ref<LightboxSwipePhase>('idle')
  const multiTouchActive = ref(false)
  const touchGestureMode = ref<TouchGestureMode | null>(null)
  const panAxis = ref<PanAxis | null>(null)
  const animationTargetIndex = ref<number | null>(null)
  const pendingNavigationIndex = ref<number | null>(null)
  const navigationReadyIndex = ref<number | null>(null)
  const suppressClick = ref(false)
  let interruptedSettling = false

  let pageLoadToken = 0
  let swipeAnimationToken = 0
  let swipeAnimationFrame: number | null = null
  let suppressClickResetTimer: number | null = null
  const swipeSamples: SwipeSample[] = []
  const trackedTouchPointers = new Map<number, TouchPointer>()
  let touchStartTarget: EventTarget | null = null
  let swipeVelocityX = 0
  let swipeVelocityY = 0
  let pageSwipeStartedFromPan = false
  let verticalGestureOffset = 0

  // 第一根指针先进入 pending；确认需要接管手势后才升级为 dragging 并捕获指针。
  const canSwipeNavigate = computed(
    () =>
      (enabled.value || verticalGestureEnabled.value) &&
      (swipePhase.value === 'idle' || swipePhase.value === 'settling') &&
      swipePointerId.value === null &&
      !multiTouchActive.value
  )

  function logGesture(label: string, event?: PointerEvent, details: Record<string, unknown> = {}) {
    if (!LIGHTBOX_GESTURE_DEBUG) {
      return
    }

    const target = event?.target
    const targetClass =
      typeof Element !== 'undefined' && target instanceof Element
        ? target.getAttribute('class')
        : null
    const targetElement =
      typeof Element !== 'undefined' && target instanceof Element
        ? `${target.tagName.toLowerCase()}${targetClass ? `.${targetClass.trim().replace(/\s+/g, '.')}` : ''}`
        : target?.constructor?.name

    console.log(`[LightboxGesture] ${label}`, {
      eventType: event?.type,
      pointerId: event?.pointerId,
      pointerType: event?.pointerType,
      clientX: event?.clientX,
      clientY: event?.clientY,
      defaultPrevented: event?.defaultPrevented,
      activeIndex: store.selection.activeIndex,
      baseIndex: baseIndex.value,
      enabled: enabled.value,
      pannable: pannable.value,
      verticalGestureEnabled: verticalGestureEnabled.value,
      canSwipeNavigate: canSwipeNavigate.value,
      phase: swipePhase.value,
      mode: touchGestureMode.value,
      animationTargetIndex: animationTargetIndex.value,
      interruptedSettling,
      swipeOffset: swipeOffset.value,
      swipePointerId: swipePointerId.value,
      trackedPointers: trackedTouchPointers.size,
      multiTouchActive: multiTouchActive.value,
      surfaceTouchAction: gestureSurfaceRef.value?.style.touchAction,
      surfaceHasPointerCapture:
        event?.pointerId !== undefined && gestureSurfaceRef.value
          ? gestureSurfaceRef.value.hasPointerCapture(event.pointerId)
          : undefined,
      targetElement,
      ...details,
    })
  }

  // 当前媒体页始终跟随轨道偏移；动画由 RAF 驱动，因此这里不依赖 CSS transition。
  const swipeViewportStyle = computed(() => ({
    transform: `translate3d(${swipeOffset.value}px, 0, 0)`,
    transition: 'none',
    willChange: swipePhase.value === 'idle' ? 'auto' : 'transform',
  }))

  // 暗房是全屏查看器，触摸始终由 Pager 仲裁，避免浏览器接管水平滑动或原生 pinch。
  const swipeGestureSurfaceStyle = computed<CSSProperties>(() => ({
    touchAction: enabled.value || pannable.value || verticalGestureEnabled.value ? 'none' : 'auto',
    overscrollBehaviorX:
      enabled.value || pannable.value || verticalGestureEnabled.value ? 'none' : 'auto',
  }))

  // 相邻页按“索引差 × 屏宽”排布，和当前媒体页共享同一个 swipeOffset。
  const swipePreviewPages = computed(() => {
    const currentIndex = baseIndex.value
    if (currentIndex === null) {
      return []
    }

    const width = Math.max(availableWidth.value, 1)
    return pageAssets.value
      .filter((page) => page.index !== currentIndex)
      .map((page) => ({
        ...page,
        style: {
          transform: `translate3d(${(page.index - currentIndex) * width + swipeOffset.value}px, 0, 0)`,
          transition: 'none',
          willChange: swipePhase.value === 'idle' ? 'auto' : 'transform',
        },
      }))
  })

  // 预热一张缩略图，让拖动过程中相邻页尽量直接可见。
  function preloadThumbnailForAsset(asset: Asset): Promise<void> {
    const url = galleryApi.getAssetThumbnailUrl(asset)
    if (!url) {
      return Promise.resolve()
    }

    return new Promise((resolve) => {
      const image = new Image()
      image.onload = () => resolve()
      // 缩略图只是视觉预览，失败时不阻断后续正式导航。
      image.onerror = () => resolve()
      image.src = url
      if (image.complete) {
        resolve()
      }
    })
  }

  // 将索引限制在当前查询结果范围内；没有结果时返回 null。
  function clamp(value: number, min: number, max: number) {
    return Math.min(Math.max(value, min), max)
  }

  // 把任意索引裁剪到当前图库可以访问的范围。
  function getValidIndex(index: number): number | null {
    if (store.totalCount <= 0) {
      return null
    }

    return clamp(index, 0, store.totalCount - 1)
  }

  // 以 baseIndex 为原点计算目标页偏移；切到下一页向左移动一屏，上一页向右移动一屏。
  function getTrackOffsetForIndex(index: number): number {
    if (baseIndex.value === null) {
      return 0
    }

    return (index - baseIndex.value) * -Math.max(availableWidth.value, 1)
  }

  // 根据手势方向取得相邻索引，越过首尾时不生成目标。
  function getSwipeTargetIndex(direction: LightboxSwipeDirection, originIndex?: number | null) {
    const currentIndex = originIndex ?? baseIndex.value
    if (currentIndex === null) {
      return null
    }

    const targetIndex = currentIndex + (direction === 'next' ? 1 : -1)
    if (targetIndex < 0 || targetIndex >= store.totalCount) {
      return null
    }

    return targetIndex
  }

  // 从缓存或分页数据中解析一个 Pager 页面。
  async function loadAssetAtIndex(index: number): Promise<Asset | null> {
    let asset = store.getAssetsInRange(index, index)[0]
    if (!asset) {
      // 相邻资源可能还没有进入前端缓存，先加载它所在的分页。
      const targetPage = Math.floor(index / store.perPage) + 1
      if (!store.isPageLoaded(targetPage)) {
        try {
          await galleryData.loadPage(targetPage)
        } catch {
          return null
        }
      }
      asset = store.getAssetsInRange(index, index)[0]
    }

    return isNavigableAsset(asset) ? asset : null
  }

  // 并发准备一组相邻页面，并用 token 丢弃 Pager 已经重置后的旧异步结果。
  async function loadPagesForIndexes(indexes: number[]) {
    const validIndexes = [
      ...new Set(indexes.map(getValidIndex).filter((index): index is number => index !== null)),
    ]
    if (validIndexes.length === 0) {
      return
    }

    const token = pageLoadToken
    const loadedPages = await Promise.all(
      validIndexes.map(async (index) => {
        const asset = await loadAssetAtIndex(index)
        return asset ? { index, asset } : null
      })
    )

    // resetPagerToIndex 会递增 token，避免旧查询把页面写回新基准。
    if (token !== pageLoadToken) {
      return
    }

    // 合并页面而不是覆盖，保证连续快扫时已经准备好的下一页仍然可用。
    const nextPages = new Map(pageAssets.value.map((page) => [page.index, page]))
    loadedPages.forEach((page) => {
      if (page) {
        nextPages.set(page.index, page)
      }
    })
    pageAssets.value = [...nextPages.values()].sort((left, right) => left.index - right.index)
    // 页面进入 Pager 后再后台预热缩略图，不阻塞手势状态机。
    void Promise.all(
      loadedPages
        .filter((page): page is SwipePage => page !== null)
        .map((page) => preloadThumbnailForAsset(page.asset))
    )
  }

  // 按当前手势方向准备目标页和再下一页，支撑连续同向快扫。
  function prepareSwipePages(direction: LightboxSwipeDirection, originIndex?: number | null) {
    const targetIndex = getSwipeTargetIndex(direction, originIndex)
    if (targetIndex === null) {
      return
    }

    const step = direction === 'next' ? 1 : -1
    void loadPagesForIndexes([targetIndex, targetIndex + step])
  }

  // 取消当前 RAF；调用方随后会递增 token，让已经排队的旧帧失效。
  function clearSwipeAnimation() {
    if (swipeAnimationFrame !== null) {
      cancelAnimationFrame(swipeAnimationFrame)
      swipeAnimationFrame = null
    }
  }

  // 清空最近一小段 pointer 轨迹，供下一次快扫重新计算速度。
  function clearSwipeVelocity() {
    swipeSamples.length = 0
    swipeVelocityX = 0
    swipeVelocityY = 0
  }

  // 记录最近 100ms 的二维位移，横向切图和纵向动作共用同一段轨迹。
  function recordSwipeSample(event: PointerEvent) {
    const time = Number.isFinite(event.timeStamp) ? event.timeStamp : performance.now()
    swipeSamples.push({ x: event.clientX, y: event.clientY, time })

    const cutoff = time - SWIPE_VELOCITY_WINDOW_MS
    // 保留窗口前最后一个采样点，让速度在窗口边界处仍然连续。
    while (swipeSamples.length > 1 && swipeSamples[1].time < cutoff) {
      swipeSamples.shift()
    }

    if (swipeSamples.length < 2) {
      swipeVelocityX = 0
      return
    }

    const first = swipeSamples[0]
    const last = swipeSamples[swipeSamples.length - 1]
    const elapsed = last.time - first.time
    if (elapsed <= 0) {
      swipeVelocityX = 0
      swipeVelocityY = 0
      return
    }

    swipeVelocityX = ((last.x - first.x) / elapsed) * 1000
    swipeVelocityY = ((last.y - first.y) / elapsed) * 1000
  }

  // 根据屏宽计算慢拖需要跨过的距离。
  function getSlowSwipeCommitDistance(): number {
    const width = Math.max(availableWidth.value, 1)
    return clamp(
      width * SWIPE_COMMIT_SLOW_DISTANCE_RATIO,
      SWIPE_COMMIT_SLOW_DISTANCE_MIN,
      SWIPE_COMMIT_SLOW_DISTANCE_MAX
    )
  }

  // 把带符号的横向速度换算成当前方向上的正速度。
  function getSwipeVelocityInDirection(direction: LightboxSwipeDirection): number {
    const velocityInDirection = direction === 'next' ? -swipeVelocityX : swipeVelocityX
    return Math.max(velocityInDirection, 0)
  }

  function getVerticalCommitDistance(): number {
    const height = Math.max(availableHeight.value, 1)
    return clamp(
      height * VERTICAL_COMMIT_DISTANCE_RATIO,
      VERTICAL_COMMIT_DISTANCE_MIN,
      VERTICAL_COMMIT_DISTANCE_MAX
    )
  }

  function getVerticalVelocityInDirection(action: LightboxVerticalGestureAction): number {
    return action === 'dismiss' ? Math.max(swipeVelocityY, 0) : Math.max(-swipeVelocityY, 0)
  }

  function getVerticalGestureOffset(deltaY: number): number {
    const maxDistance = Math.max(availableHeight.value * 0.85, VERTICAL_COMMIT_DISTANCE_MAX)
    const distance = Math.abs(deltaY)
    if (distance <= maxDistance) {
      return deltaY
    }

    // 超过视口后增加阻尼，避免拖动时把媒体完全甩出后继续扩大布局范围。
    const dampedDistance = maxDistance + (distance - maxDistance) * 0.2
    return Math.sign(deltaY) * dampedDistance
  }

  function updateVerticalGesture(event: PointerEvent, deltaY: number) {
    verticalGestureOffset = getVerticalGestureOffset(deltaY)
    const progress = clamp(Math.abs(deltaY) / getVerticalCommitDistance(), 0, 1)
    onVerticalGestureMove?.(verticalGestureOffset, progress)
    event.preventDefault()
  }

  function isTouchPointer(event: PointerEvent): boolean {
    return event.pointerType === 'touch' || event.pointerType === 'pen'
  }

  function updateTouchPointer(event: PointerEvent) {
    trackedTouchPointers.set(event.pointerId, event)
  }

  function getTouchPointerPair(): TouchPointerPair | null {
    const pointers = [...trackedTouchPointers.values()]
    if (pointers.length < 2) {
      return null
    }

    return [pointers[0], pointers[1]]
  }

  function captureTouchPointer(pointerId: number) {
    const surface = gestureSurfaceRef.value
    if (!surface || surface.hasPointerCapture(pointerId)) {
      return
    }

    surface.setPointerCapture(pointerId)
  }

  function releaseTouchPointerCaptures() {
    const surface = gestureSurfaceRef.value
    if (!surface) {
      return
    }

    for (const pointerId of trackedTouchPointers.keys()) {
      if (surface.hasPointerCapture(pointerId)) {
        surface.releasePointerCapture(pointerId)
      }
    }
  }

  // 释放 Pager 对当前指针的捕获，并清空本次手势的指针状态。
  function releaseSwipePointer(pointerId: number) {
    const hasPointerCapture = gestureSurfaceRef.value?.hasPointerCapture(pointerId) ?? false

    swipePointerId.value = null

    if (hasPointerCapture) {
      // 正常结束时主动释放；异常结束由 pointercancel 负责收尾。
      gestureSurfaceRef.value?.releasePointerCapture(pointerId)
    }
  }

  // 把 Pager 恢复到当前基准页的静止状态，但不改变基准索引和已缓存页面。
  function resetSwipeGesture() {
    // 先取消旧动画，再递增 token 使旧 RAF 不能继续写状态。
    clearSwipeAnimation()
    swipeAnimationToken += 1
    pageLoadToken += 1
    clearSwipeVelocity()
    swipePointerId.value = null
    swipePhase.value = 'idle'
    swipeOffset.value = 0
    swipeStartOffset.value = 0
    swipeGestureOriginOffset.value = 0
    swipeGestureOriginIndex.value = null
    swipeDirection.value = null
    panAxis.value = null
    animationTargetIndex.value = null
    pendingNavigationIndex.value = null
    navigationReadyIndex.value = null
    interruptedSettling = false
    pageSwipeStartedFromPan = false
    verticalGestureOffset = 0
  }

  function resetTouchTracking() {
    releaseTouchPointerCaptures()
    trackedTouchPointers.clear()
    touchStartTarget = null
    multiTouchActive.value = false
    touchGestureMode.value = null
  }

  // 切换视觉基准页，并重新准备它两侧的相邻页面。
  function resetPagerToIndex(index: number | undefined, preserveTouchTracking = false) {
    if (!preserveTouchTracking) {
      resetTouchTracking()
    }
    resetSwipeGesture()
    // 基准页是轨道坐标原点；外部导航或媒体 ready 后才更新它。
    baseIndex.value = index === undefined ? null : getValidIndex(index)
    pageAssets.value = []

    if (baseIndex.value !== null) {
      // 多准备一张同向页面，下一次快扫可以直接接上。
      void loadPagesForIndexes([baseIndex.value - 1, baseIndex.value + 1, baseIndex.value + 2])
    }
  }

  // 按当前 Pinia activeIndex 重置 Pager 的视觉状态。
  function resetSwipeVisual() {
    resetPagerToIndex(store.selection.activeIndex)
  }

  // 在触摸手势结束后短暂保留标记，阻止随后的合成 click 误触媒体。
  function scheduleSuppressClickReset(delayMs = TOUCH_GESTURE_CLICK_SUPPRESS_DURATION) {
    if (suppressClickResetTimer !== null) {
      window.clearTimeout(suppressClickResetTimer)
    }

    suppressClickResetTimer = window.setTimeout(() => {
      suppressClick.value = false
      suppressClickResetTimer = null
    }, delayMs)
  }

  // 读取并消费一次滑动产生的 click 抑制标记。
  function consumeSuppressedClick(): boolean {
    if (!suppressClick.value) {
      return false
    }

    suppressClick.value = false
    return true
  }

  // 从当前轨道位置动画到目标页；提交后等待媒体 ready 再重置 Pager 基准。
  function startSwipeAnimation(targetIndex: number, shouldCommit: boolean) {
    if (baseIndex.value === null) {
      resetSwipeVisual()
      return
    }

    const validTargetIndex = getValidIndex(targetIndex)
    if (validTargetIndex === null) {
      resetSwipeVisual()
      return
    }

    // 新动画从当前偏移接续，支持动画中再次滑动或反向回弹。
    clearSwipeAnimation()
    const token = ++swipeAnimationToken
    const fromOffset = swipeOffset.value
    const targetOffset = getTrackOffsetForIndex(validTargetIndex)
    animationTargetIndex.value = validTargetIndex

    if (shouldCommit) {
      // pendingNavigationIndex 始终表示最新一次需要提交的目标。
      if (pendingNavigationIndex.value !== validTargetIndex) {
        navigationReadyIndex.value = null
      }
      pendingNavigationIndex.value = validTargetIndex
    } else if (
      validTargetIndex === store.selection.activeIndex &&
      pendingNavigationIndex.value !== validTargetIndex
    ) {
      pendingNavigationIndex.value = null
      navigationReadyIndex.value = null
    }

    swipePhase.value = 'settling'
    const startedAt = performance.now()

    const animate = (now: number) => {
      if (token !== swipeAnimationToken) {
        return
      }

      // 用经过时间计算进度，避免 pointerdown 中断后依赖旧 CSS transition。
      const progress = clamp((now - startedAt) / SWIPE_SETTLE_DURATION, 0, 1)
      const easedProgress = 1 - Math.pow(1 - progress, 3)
      swipeOffset.value = fromOffset + (targetOffset - fromOffset) * easedProgress

      if (progress < 1) {
        swipeAnimationFrame = requestAnimationFrame(animate)
        return
      }

      swipeAnimationFrame = null
      swipeOffset.value = targetOffset

      const isAlreadyAtTarget = store.selection.activeIndex === validTargetIndex
      if (isAlreadyAtTarget && validTargetIndex === baseIndex.value) {
        // 反向手势回到当前页，不需要改动 Pinia，直接回到 idle。
        resetSwipeGesture()
        return
      }

      if (pendingNavigationIndex.value !== validTargetIndex) {
        // 动画期间已有更新的目标，旧动画不能再触发导航。
        swipePhase.value = 'idle'
        return
      }

      if (!isAlreadyAtTarget) {
        // 视觉动画完成后才更新业务 activeIndex，避免媒体内容提前跳到中央。
        navigateToIndex(validTargetIndex)
      } else if (navigationReadyIndex.value === validTargetIndex) {
        // 媒体已经在新手势期间 ready，现在可以立即提升基准页。
        resetPagerToIndex(validTargetIndex)
      }
      // 动画结束后继续保留 settling，直到媒体组件报告目标资源已挂载。
      // 期间新的 pointerdown 会从当前轨道位置继续，而不是被忽略。
    }

    swipeAnimationFrame = requestAnimationFrame(animate)
  }

  // 新手势可以中断 settling，但必须保留旧目标，后续才能继续或恢复这段动画。
  function interruptSettlingForTouch() {
    interruptedSettling = swipePhase.value === 'settling'
    if (interruptedSettling) {
      clearSwipeAnimation()
      swipeAnimationToken += 1
    }
  }

  // 新手势没有形成新目标时，恢复被中断的旧动画。
  function resumeInterruptedAnimation() {
    const targetIndex = interruptedSettling
      ? (animationTargetIndex.value ?? baseIndex.value)
      : baseIndex.value

    interruptedSettling = false
    if (targetIndex === null) {
      resetSwipeVisual()
      return
    }

    const shouldCommit =
      targetIndex !== store.selection.activeIndex || pendingNavigationIndex.value === targetIndex
    startSwipeAnimation(targetIndex, shouldCommit)
  }

  function finishSwipeTracking() {
    if (interruptedSettling) {
      resumeInterruptedAnimation()
    } else {
      resetSwipeGesture()
    }
  }

  function beginMultiTouch(event: PointerEvent) {
    const pair = getTouchPointerPair()
    if (!pair) {
      return
    }

    logGesture('multi-touch-started', event)
    onPanCancel?.()
    if (touchGestureMode.value === 'vertical') {
      onVerticalGestureCancel?.(verticalGestureOffset)
    }

    multiTouchActive.value = true
    event.preventDefault()
    resetPagerToIndex(store.selection.activeIndex, true)
    touchGestureMode.value = 'pinching'
    swipePhase.value = 'dragging'
    swipePointerId.value = pair[0].pointerId
    for (const pointerId of trackedTouchPointers.keys()) {
      captureTouchPointer(pointerId)
    }

    suppressClick.value = true
    scheduleSuppressClickReset(TOUCH_GESTURE_CLICK_SUPPRESS_DURATION)
    onPinchStart?.(pair)
  }

  // Pager 是触摸手势的唯一入口；Image 只通过回调接收平移和 pinch。
  function handleSwipePointerDown(event: PointerEvent) {
    if (!isTouchPointer(event) || !gestureSurfaceRef.value) {
      return
    }

    logGesture('pointerdown', event)

    if (canStartGesture?.(event) === false) {
      logGesture('pointerdown-ignored', event, { reason: 'native-media-control' })
      return
    }

    if (trackedTouchPointers.size > 0) {
      if (!trackedTouchPointers.has(event.pointerId) && trackedTouchPointers.size >= 2) {
        logGesture('pointerdown-ignored', event, { reason: 'too-many-pointers' })
        event.preventDefault()
        return
      }

      updateTouchPointer(event)
      if (trackedTouchPointers.size === 2 && !multiTouchActive.value) {
        beginMultiTouch(event)
      } else {
        event.preventDefault()
      }
      return
    }

    const ignoredReasons: string[] = []
    if (store.selection.activeIndex === undefined) {
      ignoredReasons.push('missing-active-index')
    }
    if (!enabled.value && !pannable.value && !verticalGestureEnabled.value) {
      ignoredReasons.push('gesture-disabled-and-not-pannable')
    }
    if (!canSwipeNavigate.value && !pannable.value && !verticalGestureEnabled.value) {
      ignoredReasons.push('swipe-navigation-unavailable')
    }
    if (ignoredReasons.length > 0) {
      logGesture('pointerdown-ignored', event, { reason: ignoredReasons })
      return
    }

    onPanCancel?.()
    interruptSettlingForTouch()
    updateTouchPointer(event)
    swipePointerId.value = event.pointerId
    swipeStartX.value = event.clientX
    swipeStartY.value = event.clientY
    swipeStartOffset.value = swipeOffset.value
    swipeGestureOriginOffset.value = swipeOffset.value
    swipeGestureOriginIndex.value =
      animationTargetIndex.value ?? baseIndex.value ?? store.selection.activeIndex ?? null
    swipeDirection.value = null
    panAxis.value = null
    pageSwipeStartedFromPan = false
    touchGestureMode.value = 'pending'
    swipePhase.value = 'pending'
    clearSwipeVelocity()
    recordSwipeSample(event)
    // pending 阶段不捕获 pointer，保留视频原生控件的 click 目标；确认手势后再捕获。
    touchStartTarget = event.target
    logGesture('pending-started', event)
  }

  function setSwipeOffsetFromDesiredOffset(nextOffset: number) {
    const width = Math.max(availableWidth.value, 1)
    const currentIndex = baseIndex.value
    if (currentIndex === null) {
      return
    }

    const minOffset = -(store.totalCount - currentIndex - 1) * width
    const maxOffset = currentIndex * width
    if (nextOffset < minOffset) {
      swipeOffset.value = minOffset + (nextOffset - minOffset) * 0.25
    } else if (nextOffset > maxOffset) {
      swipeOffset.value = maxOffset + (nextOffset - maxOffset) * 0.25
    } else {
      swipeOffset.value = nextOffset
    }
  }

  function updateSwipeOffset(event: PointerEvent, deltaX: number) {
    setSwipeOffsetFromDesiredOffset(swipeStartOffset.value + deltaX)

    const nextDirection: LightboxSwipeDirection = deltaX < 0 ? 'next' : 'previous'
    if (nextDirection !== swipeDirection.value) {
      swipeDirection.value = nextDirection
      prepareSwipePages(nextDirection, swipeGestureOriginIndex.value)
    }

    event.preventDefault()
  }

  // 图片到达水平边界后，把剩余位移接到 Pager 的轨道上，避免视觉位置跳变。
  function beginSwipeFromPanHandoff(event: PointerEvent, residualX: number) {
    const direction: LightboxSwipeDirection = residualX < 0 ? 'next' : 'previous'
    const currentTrackOffset = swipeOffset.value
    const desiredTrackOffset = currentTrackOffset + residualX

    pageSwipeStartedFromPan = true
    touchGestureMode.value = 'swiping'
    swipePhase.value = 'dragging'
    swipeStartX.value = event.clientX
    swipeStartOffset.value = desiredTrackOffset
    swipeDirection.value = direction
    clearSwipeVelocity()
    recordSwipeSample(event)
    setSwipeOffsetFromDesiredOffset(desiredTrackOffset)
    captureTouchPointer(event.pointerId)

    prepareSwipePages('next', swipeGestureOriginIndex.value)
    prepareSwipePages('previous', swipeGestureOriginIndex.value)
    prepareSwipePages(direction, swipeGestureOriginIndex.value)
    logGesture('pan-handed-off-to-swiping', event, {
      residualX,
      direction,
      desiredTrackOffset,
    })
    event.preventDefault()
  }

  function handleSwipePointerMove(event: PointerEvent) {
    if (!trackedTouchPointers.has(event.pointerId)) {
      return
    }

    updateTouchPointer(event)

    if (multiTouchActive.value) {
      event.preventDefault()
      if (touchGestureMode.value === 'pinching') {
        const pair = getTouchPointerPair()
        if (pair) {
          onPinchMove?.(pair)
        }
      }
      return
    }

    if (swipePointerId.value !== event.pointerId) {
      return
    }

    if (touchGestureMode.value === 'panning') {
      event.preventDefault()
      const panResult = onPanMove?.(event)
      if (
        panAxis.value === 'horizontal' &&
        panResult &&
        Math.abs(panResult.residualX) >= PAN_EDGE_HANDOFF_THRESHOLD
      ) {
        onPanCancel?.()
        beginSwipeFromPanHandoff(event, panResult.residualX)
      }
      return
    }

    if (touchGestureMode.value === 'vertical') {
      recordSwipeSample(event)
      const deltaY = event.clientY - swipeStartY.value
      updateVerticalGesture(event, deltaY)
      return
    }

    if (touchGestureMode.value === 'pinch-complete') {
      event.preventDefault()
      return
    }

    recordSwipeSample(event)
    const deltaX = event.clientX - swipeStartX.value
    const deltaY = event.clientY - swipeStartY.value

    if (touchGestureMode.value === 'pending') {
      if (Math.hypot(deltaX, deltaY) < SWIPE_AXIS_LOCK_THRESHOLD) {
        return
      }

      if (pannable.value) {
        touchGestureMode.value = 'panning'
        swipePhase.value = 'dragging'
        panAxis.value =
          Math.abs(deltaX) > Math.abs(deltaY) * PAN_HORIZONTAL_AXIS_RATIO
            ? 'horizontal'
            : 'vertical'
        captureTouchPointer(event.pointerId)
        onPanStart?.(event)
        logGesture('classified-as-panning', event, { deltaX, deltaY })
        event.preventDefault()
        onPanMove?.(event)
        return
      }

      const isVerticalIntent = Math.abs(deltaY) > Math.abs(deltaX) * VERTICAL_AXIS_RATIO
      if (isVerticalIntent) {
        const canStartVertical = canStartVerticalGesture?.(touchStartTarget) !== false
        if (verticalGestureEnabled.value && !canStartVertical) {
          // 视频和控件目标不参与暗房纵向导航，交还原生指针事件。
          releaseSwipePointer(event.pointerId)
          finishSwipeTracking()
          resetTouchTracking()
          return
        }

        if (verticalGestureEnabled.value) {
          touchGestureMode.value = 'vertical'
          swipePhase.value = 'dragging'
          captureTouchPointer(event.pointerId)
          suppressClick.value = true
          scheduleSuppressClickReset()
          verticalGestureOffset = 0
          onVerticalGestureMove?.(0, 0)
          logGesture('classified-as-vertical-action', event, { deltaX, deltaY })
          updateVerticalGesture(event, deltaY)
          return
        }

        touchGestureMode.value = 'pinch-complete'
        swipePhase.value = 'dragging'
        captureTouchPointer(event.pointerId)
        suppressClick.value = true
        scheduleSuppressClickReset()
        logGesture('classified-as-vertical', event, { deltaX, deltaY })
        event.preventDefault()
        return
      }

      touchGestureMode.value = 'swiping'
      swipeDirection.value = deltaX < 0 ? 'next' : 'previous'
      swipePhase.value = 'dragging'
      captureTouchPointer(event.pointerId)
      prepareSwipePages('next', swipeGestureOriginIndex.value)
      prepareSwipePages('previous', swipeGestureOriginIndex.value)
      prepareSwipePages(swipeDirection.value, swipeGestureOriginIndex.value)
      logGesture('classified-as-swiping', event, {
        deltaX,
        deltaY,
        direction: swipeDirection.value,
      })
    }

    if (touchGestureMode.value === 'swiping') {
      updateSwipeOffset(event, deltaX)
    }
  }

  function finishTouchTracking() {
    resetTouchTracking()
    resetSwipeGesture()
  }

  function handleSwipePointerUp(event: PointerEvent) {
    if (!trackedTouchPointers.has(event.pointerId)) {
      return
    }

    updateTouchPointer(event)
    logGesture('pointerup', event)

    if (multiTouchActive.value) {
      trackedTouchPointers.delete(event.pointerId)
      event.preventDefault()
      if (touchGestureMode.value === 'pinching') {
        onPinchEnd?.()
        touchGestureMode.value = 'pinch-complete'
        suppressClick.value = true
        scheduleSuppressClickReset()
      }

      if (trackedTouchPointers.size === 0) {
        multiTouchActive.value = false
        finishTouchTracking()
      }
      return
    }

    if (swipePointerId.value !== event.pointerId) {
      trackedTouchPointers.delete(event.pointerId)
      return
    }

    const mode = touchGestureMode.value
    trackedTouchPointers.delete(event.pointerId)

    if (mode === 'pending') {
      releaseSwipePointer(event.pointerId)
      finishSwipeTracking()
      const handled = onTouchTap?.(event, touchStartTarget) ?? false
      if (handled) {
        suppressClick.value = true
        scheduleSuppressClickReset()
      }
      logGesture('tap-completed', event, { handled })
      resetTouchTracking()
      return
    }

    if (mode === 'panning') {
      onPanEnd?.(event)
      releaseSwipePointer(event.pointerId)
      suppressClick.value = true
      scheduleSuppressClickReset()
      logGesture('pan-completed', event)
      finishSwipeTracking()
      resetTouchTracking()
      return
    }

    if (mode === 'vertical') {
      recordSwipeSample(event)
      const deltaY = event.clientY - swipeStartY.value
      const action: LightboxVerticalGestureAction | null =
        deltaY > 0 ? 'dismiss' : deltaY < 0 ? 'details' : null
      const rawDistance = Math.abs(deltaY)
      const isFastFling =
        action !== null && getVerticalVelocityInDirection(action) >= VERTICAL_FLING_VELOCITY
      const isSlowDrag = rawDistance >= getVerticalCommitDistance()
      const shouldCommit = action !== null && (isFastFling || isSlowDrag)
      const committedOffset = verticalGestureOffset

      releaseSwipePointer(event.pointerId)
      suppressClick.value = true
      scheduleSuppressClickReset()
      resetTouchTracking()
      interruptedSettling = false

      if (shouldCommit && action !== null) {
        onVerticalGestureCommit?.(action, committedOffset)
        resetSwipeGesture()
        return
      }

      onVerticalGestureCancel?.(committedOffset)
      finishSwipeTracking()
      resetTouchTracking()
      return
    }

    if (mode === 'pinch-complete') {
      releaseSwipePointer(event.pointerId)
      suppressClick.value = true
      scheduleSuppressClickReset()
      finishSwipeTracking()
      resetTouchTracking()
      return
    }

    if (mode !== 'swiping') {
      releaseSwipePointer(event.pointerId)
      finishSwipeTracking()
      resetTouchTracking()
      return
    }

    recordSwipeSample(event)
    const direction = swipeDirection.value
    const desiredSwipeOffset = swipeStartOffset.value + (event.clientX - swipeStartX.value)
    const rawDistance = pageSwipeStartedFromPan
      ? Math.abs(desiredSwipeOffset - swipeGestureOriginOffset.value)
      : Math.abs(event.clientX - swipeStartX.value)
    const releaseDirection =
      event.clientX < swipeStartX.value
        ? 'next'
        : event.clientX > swipeStartX.value
          ? 'previous'
          : null
    const isFastFling =
      direction !== null && getSwipeVelocityInDirection(direction) >= SWIPE_FLING_VELOCITY
    const isSlowDrag = rawDistance >= getSlowSwipeCommitDistance()
    const originIndex = swipeGestureOriginIndex.value
    const targetIndex =
      direction && originIndex !== null ? getSwipeTargetIndex(direction, originIndex) : null
    const shouldCommit =
      targetIndex !== null &&
      direction !== null &&
      (releaseDirection === null || releaseDirection === direction) &&
      (isFastFling || isSlowDrag)
    const resumeTargetIndex = interruptedSettling ? animationTargetIndex.value : null

    logGesture('swipe-completed', event, {
      direction,
      releaseDirection,
      rawDistance,
      isFastFling,
      isSlowDrag,
      targetIndex,
      shouldCommit,
    })

    releaseSwipePointer(event.pointerId)
    clearSwipeVelocity()
    suppressClick.value = true
    scheduleSuppressClickReset()
    resetTouchTracking()

    if (shouldCommit && targetIndex !== null) {
      interruptedSettling = false
      startSwipeAnimation(targetIndex, true)
      return
    }

    if (interruptedSettling && resumeTargetIndex !== null) {
      resumeInterruptedAnimation()
      return
    }

    interruptedSettling = false
    startSwipeAnimation(baseIndex.value ?? store.selection.activeIndex ?? 0, false)
  }

  function handleSwipePointerCancel(event: PointerEvent) {
    if (!trackedTouchPointers.has(event.pointerId)) {
      return
    }

    logGesture('pointercancel', event)

    if (multiTouchActive.value) {
      trackedTouchPointers.delete(event.pointerId)
      if (touchGestureMode.value === 'pinching') {
        onPinchEnd?.()
        touchGestureMode.value = 'pinch-complete'
      }
      if (trackedTouchPointers.size === 0) {
        multiTouchActive.value = false
        finishTouchTracking()
      }
      return
    }

    const mode = touchGestureMode.value
    trackedTouchPointers.delete(event.pointerId)
    if (mode === 'panning') {
      onPanCancel?.()
    }
    if (mode === 'vertical') {
      onVerticalGestureCancel?.(verticalGestureOffset)
    }
    releaseSwipePointer(event.pointerId)
    if (mode !== 'pending') {
      suppressClick.value = true
      scheduleSuppressClickReset()
    }
    finishSwipeTracking()
    resetTouchTracking()
  }

  // pending 阶段尚未捕获指针，手指移出 Pager 后仍需在 window 上完成收尾。
  function isEventInsideGestureSurface(event: PointerEvent): boolean {
    const surface = gestureSurfaceRef.value
    const target = event.target
    return !!surface && target instanceof Node && surface.contains(target)
  }

  function handleWindowPointerMove(event: PointerEvent) {
    if (!isEventInsideGestureSurface(event)) {
      handleSwipePointerMove(event)
    }
  }

  useEventListener(window, 'pointermove', handleWindowPointerMove)
  useEventListener(window, 'pointerup', handleSwipePointerUp)
  useEventListener(window, 'pointercancel', handleSwipePointerCancel)

  // 在目标媒体 ready 后提升 Pager 基准页，并清理上一轮轨道偏移。
  function completeNavigation(assetId: number) {
    const targetIndex = pendingNavigationIndex.value
    if (targetIndex === null || store.selection.activeIndex !== targetIndex) {
      return false
    }

    const targetAsset = store.getAssetsInRange(targetIndex, targetIndex)[0]
    if (!targetAsset || targetAsset.id !== assetId) {
      return false
    }

    if (
      swipePointerId.value !== null ||
      swipePhase.value === 'pending' ||
      swipePhase.value === 'dragging' ||
      multiTouchActive.value ||
      swipeAnimationFrame !== null
    ) {
      // 新手势正在使用这段轨道，先记住 ready，等它结束后再归零。
      navigationReadyIndex.value = targetIndex
      return false
    }

    // 媒体和手势都稳定后，正式把目标页变成新的坐标原点。
    resetPagerToIndex(targetIndex)
    return true
  }

  // 外部导航直接重建 Pager；手势导航期间则等待媒体 ready，避免抢走轨道控制权。
  watch(
    () => store.selection.activeIndex,
    (activeIndex) => {
      if (activeIndex === undefined) {
        if (swipePhase.value === 'idle' && pendingNavigationIndex.value === null) {
          resetPagerToIndex(undefined)
        }
        return
      }

      if (baseIndex.value === null) {
        resetPagerToIndex(activeIndex)
        return
      }

      if (multiTouchActive.value) {
        // pinch 期间由当前媒体保持手势状态，不能让外部索引更新清空已追踪指针。
        return
      }

      // 手势导航期间 Pinia 会先改变 activeIndex，但 Pager 要等媒体真正挂载后再换基准页。
      if (swipePhase.value !== 'idle' || pendingNavigationIndex.value !== null) {
        return
      }

      if (activeIndex !== baseIndex.value) {
        resetPagerToIndex(activeIndex)
      }
    },
    { immediate: true }
  )

  onUnmounted(() => {
    // 组件销毁时取消 RAF 和 click 定时器，避免异步回调访问已卸载的 Pager。
    clearSwipeAnimation()
    resetTouchTracking()
    if (suppressClickResetTimer !== null) {
      window.clearTimeout(suppressClickResetTimer)
    }
  })

  return {
    swipePhase,
    multiTouchActive,
    swipePreviewPages,
    swipeViewportStyle,
    swipeGestureSurfaceStyle,
    pendingNavigationIndex,
    canSwipeNavigate,
    consumeSuppressedClick,
    resetSwipeVisual,
    completeNavigation,
    handleSwipePointerDown,
    handleSwipePointerMove,
    handleSwipePointerUp,
    handleSwipePointerCancel,
  }
}
