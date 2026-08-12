import { computed, onUnmounted, ref, watch, type Ref } from 'vue'
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

export type LightboxSwipeDirection = 'previous' | 'next'
export type LightboxSwipePhase = 'idle' | 'dragging' | 'settling'

type ReadonlyNumberRef = Readonly<Ref<number>>
type ReadonlyBooleanRef = Readonly<Ref<boolean>>
type SwipeSample = { x: number; time: number }
type SwipePage = { index: number; asset: Asset }

interface UseLightboxSwipeNavigationOptions {
  gestureSurfaceRef: Ref<HTMLElement | null>
  availableWidth: ReadonlyNumberRef
  enabled: ReadonlyBooleanRef
  navigateToIndex: (index: number) => void
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
  const { gestureSurfaceRef, availableWidth, enabled, navigateToIndex } = options
  const store = useGalleryStore()
  const galleryData = useGalleryData()

  const baseIndex = ref<number | null>(null)
  const pageAssets = ref<SwipePage[]>([])
  const swipePointerId = ref<number | null>(null)
  const swipeStartX = ref(0)
  const swipeStartY = ref(0)
  const swipeStartOffset = ref(0)
  const swipeGestureOriginIndex = ref<number | null>(null)
  const swipeAxisLocked = ref(false)
  const swipeMoved = ref(false)
  const swipeDirection = ref<LightboxSwipeDirection | null>(null)
  const swipeOffset = ref(0)
  const swipePhase = ref<LightboxSwipePhase>('idle')
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
  let swipeVelocityX = 0

  // 只有空闲或正在回弹/前进的 Pager 才能接收新的 pointerdown。
  const canSwipeNavigate = computed(
    () =>
      enabled.value &&
      (swipePhase.value === 'idle' || swipePhase.value === 'settling') &&
      swipePointerId.value === null
  )

  // 当前媒体页始终跟随轨道偏移；动画由 RAF 驱动，因此这里不依赖 CSS transition。
  const swipeViewportStyle = computed(() => ({
    transform: `translate3d(${swipeOffset.value}px, 0, 0)`,
    transition: 'none',
    willChange: swipePhase.value === 'idle' ? 'auto' : 'transform',
  }))

  // 横向交给 Pager，纵向仍允许宿主滚动；禁用时恢复浏览器默认行为。
  const swipeGestureSurfaceStyle = computed(() => ({
    touchAction: enabled.value ? 'pan-y' : 'auto',
    overscrollBehaviorX: enabled.value ? 'none' : 'auto',
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
  }

  // 记录最近 100ms 的横向位移，用于区分快扫和慢拖。
  function recordSwipeSample(event: PointerEvent) {
    const time = Number.isFinite(event.timeStamp) ? event.timeStamp : performance.now()
    swipeSamples.push({ x: event.clientX, time })

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
    swipeVelocityX = elapsed > 0 ? ((last.x - first.x) / elapsed) * 1000 : 0
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

  // 释放 Pager 对当前指针的捕获，并清空本次手势的指针状态。
  function releaseSwipePointer(pointerId: number) {
    const hasPointerCapture = gestureSurfaceRef.value?.hasPointerCapture(pointerId) ?? false

    swipePointerId.value = null
    swipeAxisLocked.value = false
    swipeMoved.value = false

    if (hasPointerCapture) {
      // 正常结束时主动释放；lostpointercapture 只负责异常兜底。
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
    swipeAxisLocked.value = false
    swipeMoved.value = false
    swipePhase.value = 'idle'
    swipeOffset.value = 0
    swipeStartOffset.value = 0
    swipeGestureOriginIndex.value = null
    swipeDirection.value = null
    animationTargetIndex.value = null
    pendingNavigationIndex.value = null
    navigationReadyIndex.value = null
    interruptedSettling = false
  }

  // 切换视觉基准页，并重新准备它两侧的相邻页面。
  function resetPagerToIndex(index: number | undefined) {
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

  // 在 pointerup 后短暂保留“这是一次滑动”的标记，阻止随后的 click 误触媒体。
  function scheduleSuppressClickReset() {
    if (suppressClickResetTimer !== null) {
      window.clearTimeout(suppressClickResetTimer)
    }

    suppressClickResetTimer = window.setTimeout(() => {
      suppressClick.value = false
      suppressClickResetTimer = null
    }, 0)
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

  // 用户在 settling 期间没有形成新目标时，继续旧动画或回弹到原目标。
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

  // 轴向锁定失败时取消本次手势；若原来有动画，则恢复原动画。
  function cancelSwipeTracking(pointerId: number) {
    releaseSwipePointer(pointerId)
    clearSwipeVelocity()

    if (interruptedSettling) {
      resumeInterruptedAnimation()
      return
    }

    resetSwipeGesture()
  }

  // 开始一次新手势；settling 阶段允许从当前轨道位置中断旧动画。
  function handleSwipePointerDown(event: PointerEvent) {
    // 鼠标继续使用桌面端按钮，触摸/笔输入才进入拖动切图。
    if (
      event.pointerType === 'mouse' ||
      (event.pointerType !== 'touch' && event.button !== 0) ||
      !gestureSurfaceRef.value ||
      !enabled.value ||
      store.selection.activeIndex === undefined
    ) {
      return
    }

    if (!canSwipeNavigate.value) {
      return
    }

    interruptedSettling = swipePhase.value === 'settling'
    if (interruptedSettling) {
      // 保存 animationTargetIndex，后续无效手势可以恢复到旧目标。
      clearSwipeAnimation()
      swipeAnimationToken += 1
    }

    swipePointerId.value = event.pointerId
    swipeStartX.value = event.clientX
    swipeStartY.value = event.clientY
    swipeStartOffset.value = swipeOffset.value
    swipeGestureOriginIndex.value =
      animationTargetIndex.value ?? baseIndex.value ?? store.selection.activeIndex
    swipeAxisLocked.value = false
    swipeMoved.value = false
    swipeDirection.value = null
    swipePhase.value = 'dragging'
    clearSwipeVelocity()
    recordSwipeSample(event)
    // 手势开始时同时准备前后两侧，方向改变时无需重新建立状态机。
    prepareSwipePages('next', swipeGestureOriginIndex.value)
    prepareSwipePages('previous', swipeGestureOriginIndex.value)
    // 固定外层负责捕获整段手势，轨道移动不会改变后续 pointer 事件的命中区域。
    gestureSurfaceRef.value.setPointerCapture(event.pointerId)
  }

  // 根据移动轨迹锁定横轴，并把位移转换成 Pager 轨道偏移。
  function handleSwipePointerMove(event: PointerEvent) {
    if (swipePointerId.value !== event.pointerId) {
      return
    }

    recordSwipeSample(event)

    const deltaX = event.clientX - swipeStartX.value
    const deltaY = event.clientY - swipeStartY.value

    if (!swipeAxisLocked.value) {
      // 小位移先不判轴，避免手指刚落下时误触发切图。
      if (Math.hypot(deltaX, deltaY) < SWIPE_AXIS_LOCK_THRESHOLD) {
        return
      }

      if (Math.abs(deltaX) <= Math.abs(deltaY) * 1.2) {
        // 更像纵向滚动时放弃本次切图，让宿主继续处理它。
        cancelSwipeTracking(event.pointerId)
        return
      }

      swipeAxisLocked.value = true
      swipeMoved.value = true
      swipeDirection.value = deltaX < 0 ? 'next' : 'previous'
      prepareSwipePages(swipeDirection.value, swipeGestureOriginIndex.value)
    }

    // 横轴已锁定，阻止浏览器把这次手势转成水平导航或返回。
    event.preventDefault()

    if (swipeDirection.value === null) {
      return
    }

    const width = Math.max(availableWidth.value, 1)
    const currentIndex = baseIndex.value
    if (currentIndex === null) {
      return
    }

    const minOffset = -(store.totalCount - currentIndex - 1) * width
    const maxOffset = currentIndex * width
    const nextOffset = swipeStartOffset.value + deltaX
    if (nextOffset < minOffset) {
      // 到达首尾边界后保留少量阻尼，给用户明确的边界反馈。
      swipeOffset.value = minOffset + (nextOffset - minOffset) * 0.25
    } else if (nextOffset > maxOffset) {
      swipeOffset.value = maxOffset + (nextOffset - maxOffset) * 0.25
    } else {
      swipeOffset.value = nextOffset
    }

    const nextDirection: LightboxSwipeDirection = deltaX < 0 ? 'next' : 'previous'
    if (nextDirection !== swipeDirection.value) {
      // 手指中途反向时只切换目标页，不重置当前轨道偏移。
      swipeDirection.value = nextDirection
      prepareSwipePages(nextDirection, swipeGestureOriginIndex.value)
    }
  }

  // 在松手时决定提交、回弹，或继续被中断的旧动画。
  function handleSwipePointerUp(event: PointerEvent) {
    if (swipePointerId.value !== event.pointerId) {
      return
    }

    recordSwipeSample(event)

    const wasHorizontalSwipe = swipeAxisLocked.value && swipeMoved.value
    const rawDistance = Math.abs(event.clientX - swipeStartX.value)
    const direction = swipeDirection.value
    const releaseDirection =
      event.clientX < swipeStartX.value
        ? 'next'
        : event.clientX > swipeStartX.value
          ? 'previous'
          : null
    // 快扫看速度，慢拖看距离；两者共用同一个目标方向。
    const isFastFling =
      direction !== null && getSwipeVelocityInDirection(direction) >= SWIPE_FLING_VELOCITY
    const isSlowDrag = rawDistance >= getSlowSwipeCommitDistance()
    const originIndex = swipeGestureOriginIndex.value
    const targetIndex =
      direction && originIndex !== null ? getSwipeTargetIndex(direction, originIndex) : null
    const shouldCommit =
      wasHorizontalSwipe &&
      targetIndex !== null &&
      direction !== null &&
      (releaseDirection === null || releaseDirection === direction) &&
      (isFastFling || isSlowDrag)
    const resumeTargetIndex = interruptedSettling ? animationTargetIndex.value : null

    // 先释放指针捕获，再启动 RAF，避免旧 pointer 生命周期影响新动画。
    releaseSwipePointer(event.pointerId)
    clearSwipeVelocity()

    if (!wasHorizontalSwipe) {
      if (interruptedSettling) {
        resumeInterruptedAnimation()
      } else {
        resetSwipeGesture()
      }
      return
    }

    // 无论最终提交还是回弹，都屏蔽这个手势生成的 click。
    suppressClick.value = true
    scheduleSuppressClickReset()

    if (shouldCommit && targetIndex !== null) {
      // 形成了有效目标，开始向相邻页推进。
      interruptedSettling = false
      startSwipeAnimation(targetIndex, true)
      return
    }

    if (interruptedSettling && resumeTargetIndex !== null) {
      // 新手势没有形成新目标，恢复第一次滑动的目标。
      resumeInterruptedAnimation()
      return
    }

    interruptedSettling = false
    // 普通未达阈值的拖动回到当前基准页。
    startSwipeAnimation(baseIndex.value ?? store.selection.activeIndex ?? 0, false)
  }

  // 处理系统取消或浏览器接管后的手势收尾。
  function handleSwipePointerCancel(event: PointerEvent) {
    if (swipePointerId.value !== event.pointerId) {
      return
    }

    const wasHorizontalSwipe = swipeAxisLocked.value && swipeMoved.value
    releaseSwipePointer(event.pointerId)
    clearSwipeVelocity()

    if (wasHorizontalSwipe) {
      suppressClick.value = true
      scheduleSuppressClickReset()
    }

    if (interruptedSettling) {
      resumeInterruptedAnimation()
    } else {
      resetSwipeGesture()
    }
  }

  // 处理指针捕获意外丢失，统一复用 pointercancel 的恢复逻辑。
  function handleSwipeLostPointerCapture(event: PointerEvent) {
    if (swipePointerId.value === event.pointerId) {
      handleSwipePointerCancel(event)
    }
  }

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
      swipePhase.value === 'dragging' ||
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

  // 图片进入可平移放大状态时暂停 Pager 手势，避免两个拖拽状态机抢同一个 pointer。
  watch(enabled, (isEnabled) => {
    if (!isEnabled && swipePhase.value !== 'idle') {
      resetSwipeVisual()
    }
  })

  onUnmounted(() => {
    // 组件销毁时取消 RAF 和 click 定时器，避免异步回调访问已卸载的 Pager。
    clearSwipeAnimation()
    if (suppressClickResetTimer !== null) {
      window.clearTimeout(suppressClickResetTimer)
    }
  })

  return {
    swipePhase,
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
    handleSwipeLostPointerCapture,
  }
}
