<script setup lang="ts">
import { computed, nextTick, onUnmounted, ref, watch } from 'vue'
import { useElementSize } from '@vueuse/core'
import { galleryApi } from '../../api'
import { useGalleryData, useGalleryLightbox } from '../../composables'
import { useGalleryStore } from '../../store'
import type { Asset } from '../../types'
import { isGalleryTouchInput, normalizeGalleryInputType } from '../../input'
import { useI18n } from '@/composables/useI18n'

const VIEWPORT_PADDING = 0
const ZOOM_STEP = 1.1
// 缩放吸附容差：实际缩放比例接近 fitScale 的 1.02 倍以内时，自动吸附回适合模式
const FIT_MODE_SNAP_RATIO = 1.02
const DRAG_THRESHOLD = 4
const MIN_ACTUAL_ZOOM = 0.05
const MAX_ACTUAL_ZOOM = 5
/** 触摸双击缩放的过渡时长；桌面点击不使用这段动画。 */
const TOUCH_ZOOM_ANIMATION_DURATION_MS = 240
// 只用松手前一小段轨迹估算触摸平移速度，避免把停顿前的慢速拖动带入惯性。
const PAN_VELOCITY_WINDOW_MS = 100
// 低于这个速度时直接停下，避免尾端出现看不见但持续占用 RAF 的微小移动。
const PAN_INERTIA_MIN_VELOCITY = 30
// 每 16.67ms 保留约 92% 的速度，形成自然的减速尾巴。
const PAN_INERTIA_DECAY = 0.92
const PAN_INERTIA_MAX_DURATION_MS = 900

interface ZoomAnchor {
  pointerX: number
  pointerY: number
  imageX: number
  imageY: number
}

interface StagePoint {
  x: number
  y: number
}

interface TouchZoomTransition {
  startScale: number
  targetScale: number
  startPosition: StagePoint
  targetPosition: StagePoint
}

// 复原代理以缩略图的自然像素为基准，避免把缩略图也布局成原图的 8K 尺寸。
interface ThumbnailRestoreTransition {
  startScale: number
  targetScale: number
  startCenter: StagePoint
  targetCenter: StagePoint
  thumbnailWidth: number
  thumbnailHeight: number
}

interface TouchPointer {
  pointerId: number
  clientX: number
  clientY: number
}

interface PanSample {
  x: number
  y: number
  time: number
}

interface PanMoveResult {
  /** 图片到达水平边界后仍未消费的手指位移。 */
  residualX: number
  /** 图片到达垂直边界后仍未消费的手指位移。 */
  residualY: number
}

type TouchPointerPair = readonly [TouchPointer, TouchPointer]

const { t } = useI18n()
const store = useGalleryStore()
const lightbox = useGalleryLightbox()
const galleryData = useGalleryData()
const emit = defineEmits<{
  ready: [assetId: number]
  'pannable-change': [pannable: boolean]
}>()

const imageError = ref(false)
const originalLoaded = ref(false)
// 仅在同一张图原图 load 完成时启用淡入；切图和复原交接不做过渡，避免叠帧。
const originalOpacityTransition = ref(false)
// 复原动画期间用缩略图托底，避免原图重新栅格化时露出空白块。
const touchZoomRestoreFallback = ref(false)
const autoRecovering = ref(false)
// 实际渲染的资产 id；切图时滞后于 selection，直到目标缩略图 decode 完成再切换。
const displayAssetId = ref<number | null>(null)
let displaySwapToken = 0
const lightboxRootRef = ref<HTMLElement | null>(null)
const viewportRef = ref<HTMLElement | null>(null)
const stageRef = ref<HTMLElement | null>(null)
const thumbnailImageRef = ref<HTMLImageElement | null>(null)
const originalImageRef = ref<HTMLImageElement | null>(null)
// 代理层独立于可滚动 stage，只有缩小复原时才挂载并接管画面。
const restoreThumbnailRef = ref<HTMLImageElement | null>(null)
const restoreThumbnailActive = ref(false)
const activePointerId = ref<number | null>(null)
const dragStartX = ref(0)
const dragStartY = ref(0)
const dragStartScrollLeft = ref(0)
const dragStartScrollTop = ref(0)
const dragMoved = ref(false)
const panSamples: PanSample[] = []
let panAnimationFrame: number | null = null
let panAnimationToken = 0
// 鼠标拖拽结束后屏蔽紧随而来的 click，触摸手势由 Pager 统一处理。
const suppressStageClick = ref(false)
let suppressStageClickResetTimer: number | null = null
const pinchActive = ref(false)
let pinchStartDistance = 0
let pinchStartScale = 1
let pinchAnchorImageX = 0
let pinchAnchorImageY = 0
let pinchAnimationFrame: number | null = null
let zoomRenderToken = 0
let pinchFinishing = false
let pinchPointerPair: TouchPointerPair | null = null
let touchZoomAnimationFrame: number | null = null
let touchZoomAnimationToken = 0
let touchZoomTransition: TouchZoomTransition | null = null

const { width, height } = useElementSize(viewportRef)

const availableWidth = computed(() => width.value)
const availableHeight = computed(() => height.value)
const viewportInnerWidth = computed(() => Math.max(availableWidth.value - VIEWPORT_PADDING * 2, 1))
const viewportInnerHeight = computed(() =>
  Math.max(availableHeight.value - VIEWPORT_PADDING * 2, 1)
)

// 从 Pinia 当前索引读取业务资产；displayAsset 负责跟随加载进度实际渲染。
const currentAsset = computed(() => {
  const currentIdx = store.selection.activeIndex
  if (currentIdx === undefined) {
    return null
  }

  return store.getAssetsInRange(currentIdx, currentIdx)[0] ?? null
})

// 在已加载页面中按 id 查找实际渲染对象，避免 displayAsset 直接依赖当前索引。
function findLoadedAssetById(assetId: number): Asset | null {
  for (const pageAssets of store.paginatedAssets.values()) {
    const found = pageAssets.find((asset) => asset.id === assetId)
    if (found) {
      return found
    }
  }
  return null
}

const displayAsset = computed(() => {
  if (displayAssetId.value === null) {
    return null
  }
  return findLoadedAssetById(displayAssetId.value)
})

const thumbnailUrl = computed(() => {
  if (!displayAsset.value) return ''
  return galleryApi.getAssetThumbnailUrl(displayAsset.value)
})

const originalUrl = computed(() => {
  if (!displayAsset.value) return ''
  return galleryData.getAssetUrl(displayAsset.value)
})

const fitMode = computed(() => store.lightbox.fitMode)
const actualZoom = computed(() => store.lightbox.zoom)
const rotationDegrees = computed(() => store.lightbox.rotationDegrees)

const imageWidth = computed(() => displayAsset.value?.width || 0)
const imageHeight = computed(() => displayAsset.value?.height || 0)
const hasImageDimensions = computed(() => imageWidth.value > 0 && imageHeight.value > 0)
const normalizedRotationDegrees = computed(() => ((rotationDegrees.value % 360) + 360) % 360)
const isQuarterTurn = computed(() => normalizedRotationDegrees.value % 180 !== 0)
const visualImageWidth = computed(() =>
  isQuarterTurn.value ? imageHeight.value : imageWidth.value
)
const visualImageHeight = computed(() =>
  isQuarterTurn.value ? imageWidth.value : imageHeight.value
)

const fitScale = computed(() => {
  if (
    !hasImageDimensions.value ||
    viewportInnerWidth.value <= 0 ||
    viewportInnerHeight.value <= 0
  ) {
    return 1
  }

  return Math.min(
    viewportInnerWidth.value / visualImageWidth.value,
    viewportInnerHeight.value / visualImageHeight.value,
    1
  )
})

const displayScale = computed(() => {
  if (fitMode.value === 'contain') {
    return fitScale.value
  }

  return actualZoom.value
})

const renderWidth = computed(() => {
  if (!hasImageDimensions.value) {
    return Math.max(viewportInnerWidth.value, 1)
  }

  return Math.max(visualImageWidth.value * displayScale.value, 1)
})

const renderHeight = computed(() => {
  if (!hasImageDimensions.value) {
    return Math.max(viewportInnerHeight.value, 1)
  }

  return Math.max(visualImageHeight.value * displayScale.value, 1)
})

const canvasWidth = computed(
  () => Math.max(renderWidth.value, viewportInnerWidth.value) + VIEWPORT_PADDING * 2
)
const canvasHeight = computed(
  () => Math.max(renderHeight.value, viewportInnerHeight.value) + VIEWPORT_PADDING * 2
)

const canvasStyle = computed(() => ({
  width: `${canvasWidth.value}px`,
  height: `${canvasHeight.value}px`,
  padding: `${VIEWPORT_PADDING}px`,
}))

const isPannable = computed(
  () =>
    fitMode.value === 'actual' &&
    (renderWidth.value > viewportInnerWidth.value || renderHeight.value > viewportInnerHeight.value)
)

// 把“图片是否会抢走拖拽 pointer”同步给外层 Pager。
watch(
  isPannable,
  (pannable) => {
    emit('pannable-change', pannable)
  },
  { immediate: true }
)

const isDragging = computed(() => activePointerId.value !== null)

const stageCursor = computed(() => {
  if (!displayAsset.value || imageError.value) {
    return 'default'
  }

  if (fitMode.value === 'contain') {
    return 'zoom-in'
  }

  if (isPannable.value) {
    return isDragging.value ? 'grabbing' : 'grab'
  }

  return 'zoom-out'
})

const stageStyle = computed(() => ({
  width: `${renderWidth.value}px`,
  height: `${renderHeight.value}px`,
  cursor: stageCursor.value,
  // 代理层显示时保留 stage 的布局尺寸，但不让它的巨大图层参与绘制。
  visibility: restoreThumbnailActive.value ? 'hidden' : undefined,
}))

function buildTouchZoomTransform(transition: TouchZoomTransition, easedProgress: number): string {
  const currentScale =
    transition.startScale + (transition.targetScale - transition.startScale) * easedProgress
  const currentX =
    transition.startPosition.x +
    (transition.targetPosition.x - transition.startPosition.x) * easedProgress
  const currentY =
    transition.startPosition.y +
    (transition.targetPosition.y - transition.startPosition.y) * easedProgress
  const visualScale = transition.startScale > 0 ? currentScale / transition.startScale : 1

  return `matrix(${visualScale}, 0, 0, ${visualScale}, ${currentX - transition.startPosition.x}, ${currentY - transition.startPosition.y})`
}

// 双击动画只改变视觉层；冻结布局期间关闭内部滚动，避免缩放过程触发浏览器的滚动边界裁剪。
function applyTouchZoomTransform(transition: TouchZoomTransition, easedProgress: number) {
  const viewport = viewportRef.value
  const stage = stageRef.value
  if (!viewport || !stage) {
    return
  }

  viewport.style.overflow = 'hidden'
  stage.style.transformOrigin = '0 0'
  stage.style.willChange = 'transform'
  stage.style.transform = buildTouchZoomTransform(transition, easedProgress)
}

function clearTouchZoomAnimationStyles() {
  viewportRef.value?.style.removeProperty('overflow')
  stageRef.value?.style.removeProperty('transform')
  stageRef.value?.style.removeProperty('transform-origin')
  stageRef.value?.style.removeProperty('will-change')
}

function getRootRelativePoint(point: StagePoint): StagePoint {
  // 动画目标使用 viewport 坐标，代理层却挂在外层 root，需要先消除两者的坐标原点差异。
  const root = lightboxRootRef.value
  const viewport = viewportRef.value
  if (!root || !viewport) {
    return point
  }

  const rootRect = root.getBoundingClientRect()
  const viewportRect = viewport.getBoundingClientRect()
  return {
    x: point.x + viewportRect.left - rootRect.left,
    y: point.y + viewportRect.top - rootRect.top,
  }
}

function applyThumbnailRestoreTransform(
  transition: ThumbnailRestoreTransition,
  easedProgress: number
) {
  const image = restoreThumbnailRef.value
  if (!image) {
    return
  }

  viewportRef.value?.style.setProperty('overflow', 'hidden')

  const currentScale =
    transition.startScale + (transition.targetScale - transition.startScale) * easedProgress
  const currentCenter = {
    x:
      transition.startCenter.x +
      (transition.targetCenter.x - transition.startCenter.x) * easedProgress,
    y:
      transition.startCenter.y +
      (transition.targetCenter.y - transition.startCenter.y) * easedProgress,
  }
  const scaleX =
    (imageWidth.value * currentScale) / Math.max(transition.thumbnailWidth, Number.EPSILON)
  const scaleY =
    (imageHeight.value * currentScale) / Math.max(transition.thumbnailHeight, Number.EPSILON)

  // 代理图的盒子尺寸固定为缩略图原生尺寸；每一帧只更新合成 transform，避免触发布局重算。
  image.style.left = `${currentCenter.x}px`
  image.style.top = `${currentCenter.y}px`
  image.style.transform = `translate3d(-50%, -50%, 0) rotate(${normalizedRotationDegrees.value}deg) scale(${scaleX}, ${scaleY})`
}

function clearThumbnailRestoreStyles() {
  // 先恢复正常 stage，再由 Vue 在下一次 DOM 更新中移除代理节点，避免交接时出现空白帧。
  restoreThumbnailActive.value = false
  const image = restoreThumbnailRef.value
  image?.style.removeProperty('left')
  image?.style.removeProperty('top')
  image?.style.removeProperty('width')
  image?.style.removeProperty('height')
  image?.style.removeProperty('transform')
  image?.style.removeProperty('transform-origin')
  image?.style.removeProperty('will-change')
  image?.style.removeProperty('opacity')
}

const imageLayerStyle = computed(() => {
  const layerWidth = hasImageDimensions.value
    ? Math.max(imageWidth.value * displayScale.value, 1)
    : renderWidth.value
  const layerHeight = hasImageDimensions.value
    ? Math.max(imageHeight.value * displayScale.value, 1)
    : renderHeight.value

  return {
    width: `${layerWidth}px`,
    height: `${layerHeight}px`,
    transform: `translate(-50%, -50%) rotate(${normalizedRotationDegrees.value}deg)`,
  }
})

const originalLayerStyle = computed(() => ({
  ...imageLayerStyle.value,
  opacity: originalLoaded.value && !touchZoomRestoreFallback.value ? 1 : 0,
  transition:
    touchZoomRestoreFallback.value || !originalOpacityTransition.value
      ? 'none'
      : 'opacity 200ms',
}))

// 等待图片可以被浏览器绘制；不重新请求资源，也不人为延长显示时间。
async function waitForImageReady(image: HTMLImageElement | null): Promise<boolean> {
  if (!image) {
    return false
  }

  const source = image.currentSrc || image.src
  if (!image.complete) {
    await new Promise<void>((resolve) => {
      let settled = false
      const settle = () => {
        if (settled) {
          return
        }
        settled = true
        image.removeEventListener('load', settle)
        image.removeEventListener('error', settle)
        resolve()
      }

      image.addEventListener('load', settle)
      image.addEventListener('error', settle)
      // 监听器注册和检查 complete 之间可能已经完成，避免遗漏 load 事件。
      if (image.complete) {
        settle()
      }
    })
  }

  if (image.naturalWidth <= 0 || (image.currentSrc || image.src) !== source) {
    return false
  }

  try {
    // decode 只等待当前资源完成解码，不修改 src，也不会重新发起网络请求。
    await image.decode()
  } catch {
    // load 已经成功且有像素时，decode 的拒绝不应阻止当前图片显示。
  }

  return image.naturalWidth > 0 && (image.currentSrc || image.src) === source
}

function waitForOriginalImageReady(): Promise<boolean> {
  return waitForImageReady(originalImageRef.value)
}

// 给缩略图留出一个实际绘制帧；这是渲染同步点，不是人为添加的动画延时。
function waitForThumbnailPaint(): Promise<void> {
  return new Promise((resolve) => {
    requestAnimationFrame(() => {
      requestAnimationFrame(() => resolve())
    })
  })
}

const zoomIndicator = computed(() => {
  if (fitMode.value === 'contain') {
    return t('gallery.lightbox.image.fitIndicator', { percent: Math.round(fitScale.value * 100) })
  }

  return `${Math.round(actualZoom.value * 100)}%`
})

// 预热目标缩略图，切换时先保证稳定的低清底图。
function preloadThumbnailForAsset(asset: Asset): Promise<void> {
  const url = galleryApi.getAssetThumbnailUrl(asset)
  if (!url) {
    return Promise.resolve()
  }

  return new Promise((resolve) => {
    const image = new Image()
    image.onload = () => resolve()
    image.onerror = () => resolve()
    image.src = url
    if (image.complete) {
      resolve()
    }
  })
}

// 切换当前渲染资产，并在 DOM 更新后通知 Pager 媒体已经可接管中心位置。
async function commitDisplayAsset(assetId: number) {
  originalOpacityTransition.value = false
  touchZoomRestoreFallback.value = false
  displayAssetId.value = assetId
  originalLoaded.value = false
  imageError.value = false
  // 切图时清掉旧图的平移和 click 抑制状态，避免把上一张图的交互带过来。
  resetPinchTracking()
  resetPointerState()
  clearStageClickSuppression()

  await nextTick()
  // 等 viewport 重新布局后再校正滚动位置。
  syncViewportPosition()
  // Pager 用这个事件结束 pending navigation，而不是依赖固定延时。
  emit('ready', assetId)
}

// 跟随 Pinia activeIndex 切换图片；缩略图准备好后再替换 displayAsset。
watch(
  () => ({
    targetId: currentAsset.value?.id,
    activeIndex: store.selection.activeIndex,
  }),
  async ({ targetId, activeIndex }) => {
    if (activeIndex === undefined) {
      // 灯箱关闭或没有焦点时清空当前渲染对象。
      resetPinchTracking()
      resetPointerState()
      clearStageClickSuppression()
      displayAssetId.value = null
      return
    }

    if (targetId === undefined) {
      return
    }

    if (displayAssetId.value === null) {
      // 首次挂载无需等待旧图，直接显示当前资源。
      await commitDisplayAsset(targetId)
      return
    }

    if (displayAssetId.value === targetId) {
      return
    }

    const asset = currentAsset.value
    if (!asset) {
      return
    }

    const token = ++displaySwapToken
    // 先让目标缩略图进入缓存，避免 Pager 归零后出现空白帧。
    await preloadThumbnailForAsset(asset)
    if (token !== displaySwapToken) {
      return
    }
    if (currentAsset.value?.id !== targetId) {
      // 等待期间如果用户又滑到了别的资源，丢弃这次过期切换。
      return
    }

    await commitDisplayAsset(targetId)
  },
  { immediate: true, flush: 'post' }
)

watch(
  [availableWidth, availableHeight],
  async () => {
    if (touchZoomTransition) {
      // 尺寸变化会改变目标适屏矩形；取消旧过渡，等待下一次手势重新计算。
      cancelTouchZoomAnimation()
    }

    // 窗口尺寸变化后重新计算适屏位置或裁剪放大后的滚动范围。
    await nextTick()
    if (fitMode.value === 'contain') {
      syncViewportPosition()
      return
    }

    clampViewportScroll()
  },
  { flush: 'post' }
)

watch(
  () => normalizedRotationDegrees.value,
  async () => {
    // 旋转会改变画布尺寸，先清理拖拽，再按新尺寸校正位置。
    resetPinchTracking()
    resetPointerState()
    clearStageClickSuppression()

    await nextTick()
    syncViewportPosition()
  },
  { flush: 'post' }
)

function clamp(value: number, min: number, max: number): number {
  return Math.min(Math.max(value, min), max)
}

function clampActualZoom(zoom: number): number {
  return clamp(zoom, MIN_ACTUAL_ZOOM, MAX_ACTUAL_ZOOM)
}

// 将 viewport 的滚动位置限制在当前画布范围内。
function clampViewportScroll() {
  const viewport = viewportRef.value
  if (!viewport) return

  viewport.scrollLeft = clamp(
    viewport.scrollLeft,
    0,
    Math.max(viewport.scrollWidth - viewport.clientWidth, 0)
  )
  viewport.scrollTop = clamp(
    viewport.scrollTop,
    0,
    Math.max(viewport.scrollHeight - viewport.clientHeight, 0)
  )
}

// 设置滚动位置并统一做边界裁剪。
function setViewportScroll(left: number, top: number) {
  const viewport = viewportRef.value
  if (!viewport) return

  viewport.scrollLeft = clamp(left, 0, Math.max(viewport.scrollWidth - viewport.clientWidth, 0))
  viewport.scrollTop = clamp(top, 0, Math.max(viewport.scrollHeight - viewport.clientHeight, 0))
}

function cancelPanInertia() {
  if (panAnimationFrame !== null) {
    cancelAnimationFrame(panAnimationFrame)
    panAnimationFrame = null
  }

  panAnimationToken += 1
  panSamples.length = 0
}

function getPanSampleTime(event: PointerEvent): number {
  return Number.isFinite(event.timeStamp) ? event.timeStamp : performance.now()
}

function recordPanSample(event: PointerEvent) {
  const time = getPanSampleTime(event)
  panSamples.push({ x: event.clientX, y: event.clientY, time })

  const cutoff = time - PAN_VELOCITY_WINDOW_MS
  while (panSamples.length > 1 && panSamples[1].time < cutoff) {
    panSamples.shift()
  }
}

function getPanVelocity() {
  if (panSamples.length < 2) {
    return { x: 0, y: 0 }
  }

  const first = panSamples[0]
  const last = panSamples[panSamples.length - 1]
  const elapsed = last.time - first.time
  if (elapsed <= 0) {
    return { x: 0, y: 0 }
  }

  // viewport 的滚动方向与手指移动方向相反。
  return {
    x: ((first.x - last.x) / elapsed) * 1000,
    y: ((first.y - last.y) / elapsed) * 1000,
  }
}

function startPanInertia(initialVelocityX: number, initialVelocityY: number) {
  cancelPanInertia()

  if (!viewportRef.value || !isPannable.value) {
    return
  }

  let velocityX = initialVelocityX
  let velocityY = initialVelocityY
  if (Math.hypot(velocityX, velocityY) < PAN_INERTIA_MIN_VELOCITY) {
    return
  }

  const token = panAnimationToken
  const startedAt = performance.now()
  let previousTime = startedAt

  const animate = (now: number) => {
    if (token !== panAnimationToken) {
      return
    }

    const viewport = viewportRef.value
    if (!viewport || !isPannable.value) {
      panAnimationFrame = null
      return
    }

    const elapsed = clamp(now - previousTime, 1, 32)
    previousTime = now

    const maxScrollLeft = Math.max(viewport.scrollWidth - viewport.clientWidth, 0)
    const maxScrollTop = Math.max(viewport.scrollHeight - viewport.clientHeight, 0)
    const nextScrollLeft = clamp(
      viewport.scrollLeft + (velocityX * elapsed) / 1000,
      0,
      maxScrollLeft
    )
    const nextScrollTop = clamp(viewport.scrollTop + (velocityY * elapsed) / 1000, 0, maxScrollTop)

    viewport.scrollLeft = nextScrollLeft
    viewport.scrollTop = nextScrollTop

    // 到达边界后只停止撞向边界的轴，另一轴仍可继续惯性移动。
    if (
      (nextScrollLeft === 0 && velocityX < 0) ||
      (nextScrollLeft === maxScrollLeft && velocityX > 0)
    ) {
      velocityX = 0
    }
    if (
      (nextScrollTop === 0 && velocityY < 0) ||
      (nextScrollTop === maxScrollTop && velocityY > 0)
    ) {
      velocityY = 0
    }

    const decay = Math.pow(PAN_INERTIA_DECAY, elapsed / 16.67)
    velocityX *= decay
    velocityY *= decay

    const expired = now - startedAt >= PAN_INERTIA_MAX_DURATION_MS
    if (expired || Math.hypot(velocityX, velocityY) < PAN_INERTIA_MIN_VELOCITY) {
      panAnimationFrame = null
      return
    }

    panAnimationFrame = requestAnimationFrame(animate)
  }

  panAnimationFrame = requestAnimationFrame(animate)
}

function getCurrentScale(): number {
  return fitMode.value === 'contain' ? fitScale.value : actualZoom.value
}

// 根据当前 fitMode 把 viewport 放回适屏原点或实际大小的居中位置。
function syncViewportPosition() {
  const viewport = viewportRef.value
  if (!viewport) return

  if (fitMode.value === 'contain') {
    // 适屏模式不保留旧的放大滚动位置。
    viewport.scrollLeft = 0
    viewport.scrollTop = 0
    return
  }

  // 实际大小模式把画布中心对齐到 viewport 中心。
  setViewportScroll(
    Math.max((canvasWidth.value - availableWidth.value) / 2, 0),
    Math.max((canvasHeight.value - availableHeight.value) / 2, 0)
  )
}

function getViewportCenterClientPoint() {
  const viewport = viewportRef.value
  if (!viewport) return null

  const rect = viewport.getBoundingClientRect()
  return {
    clientX: rect.left + rect.width / 2,
    clientY: rect.top + rect.height / 2,
  }
}

/**
 * 计算以 (clientX, clientY) 为锚点的缩放锚信息。
 * 缩放后调用 restoreZoomAnchor 可使该像素点在视口中的位置保持不变，
 * 实现「以鼠标/视口中心为原点」的平滑缩放体验。
 */
function getZoomAnchor(clientX: number, clientY: number, scale: number): ZoomAnchor | null {
  const viewport = viewportRef.value
  const stage = stageRef.value
  if (!viewport || !stage || scale <= 0 || !hasImageDimensions.value) {
    return null
  }

  const viewportRect = viewport.getBoundingClientRect()
  const stageRect = stage.getBoundingClientRect()
  const pointerX = clamp(clientX - viewportRect.left, 0, viewportRect.width)
  const pointerY = clamp(clientY - viewportRect.top, 0, viewportRect.height)
  const stageOffsetLeft = viewport.scrollLeft + (stageRect.left - viewportRect.left)
  const stageOffsetTop = viewport.scrollTop + (stageRect.top - viewportRect.top)

  return {
    pointerX,
    pointerY,
    imageX: clamp(
      (viewport.scrollLeft + pointerX - stageOffsetLeft) / scale,
      0,
      visualImageWidth.value
    ),
    imageY: clamp(
      (viewport.scrollTop + pointerY - stageOffsetTop) / scale,
      0,
      visualImageHeight.value
    ),
  }
}

function getStageViewportPosition(): StagePoint | null {
  const viewport = viewportRef.value
  const stage = stageRef.value
  if (!viewport || !stage) {
    return null
  }

  const viewportRect = viewport.getBoundingClientRect()
  const stageRect = stage.getBoundingClientRect()
  return {
    x: stageRect.left - viewportRect.left,
    y: stageRect.top - viewportRect.top,
  }
}

// 在 stage 没有自身 transform 时，读取它在 viewport 内容坐标中的位置。
function getStageContentPosition(): StagePoint | null {
  const viewport = viewportRef.value
  const stage = stageRef.value
  if (!viewport || !stage) {
    return null
  }

  const viewportRect = viewport.getBoundingClientRect()
  const stageRect = stage.getBoundingClientRect()
  return {
    x: viewport.scrollLeft + stageRect.left - viewportRect.left,
    y: viewport.scrollTop + stageRect.top - viewportRect.top,
  }
}

function getZoomLayout(scale: number) {
  const renderWidth = Math.max(visualImageWidth.value * scale, 1)
  const renderHeight = Math.max(visualImageHeight.value * scale, 1)

  return {
    renderWidth,
    renderHeight,
  }
}

function constrainStagePosition(position: number, imageSize: number, viewportSize: number): number {
  if (imageSize <= viewportSize) {
    return (viewportSize - imageSize) / 2
  }

  return clamp(position, viewportSize - imageSize, 0)
}

// 计算动画终点；图片小于视口时保持适屏居中，大于视口时才允许在边界内平移。
function getTouchZoomTargetPosition(anchor: ZoomAnchor, scale: number): StagePoint {
  const layout = getZoomLayout(scale)
  return {
    x: constrainStagePosition(
      anchor.pointerX - anchor.imageX * scale,
      layout.renderWidth,
      availableWidth.value
    ),
    y: constrainStagePosition(
      anchor.pointerY - anchor.imageY * scale,
      layout.renderHeight,
      availableHeight.value
    ),
  }
}

async function restoreZoomAnchor(anchor: ZoomAnchor, scale: number, renderToken?: number) {
  await nextTick()

  if (renderToken !== undefined && renderToken !== zoomRenderToken) {
    return
  }

  const viewport = viewportRef.value
  const stage = stageRef.value
  if (!viewport || !stage) {
    return
  }

  const viewportRect = viewport.getBoundingClientRect()
  const stageRect = stage.getBoundingClientRect()
  const stageOffsetLeft = viewport.scrollLeft + (stageRect.left - viewportRect.left)
  const stageOffsetTop = viewport.scrollTop + (stageRect.top - viewportRect.top)

  setViewportScroll(
    stageOffsetLeft + anchor.imageX * scale - anchor.pointerX,
    stageOffsetTop + anchor.imageY * scale - anchor.pointerY
  )
}

function isTouchPointerEvent(event: PointerEvent): boolean {
  return isGalleryTouchInput(normalizeGalleryInputType(event.pointerType))
}

function getPinchDistance(first: TouchPointer, second: TouchPointer): number {
  return Math.hypot(second.clientX - first.clientX, second.clientY - first.clientY)
}

function getPinchCenter(
  first: TouchPointer,
  second: TouchPointer
): { clientX: number; clientY: number } {
  return {
    clientX: (first.clientX + second.clientX) / 2,
    clientY: (first.clientY + second.clientY) / 2,
  }
}

function getViewportRelativePoint(clientX: number, clientY: number) {
  const viewport = viewportRef.value
  if (!viewport) {
    return null
  }

  const rect = viewport.getBoundingClientRect()
  return {
    pointerX: clamp(clientX - rect.left, 0, rect.width),
    pointerY: clamp(clientY - rect.top, 0, rect.height),
  }
}

function cancelPinchAnimation() {
  if (pinchAnimationFrame !== null) {
    cancelAnimationFrame(pinchAnimationFrame)
    pinchAnimationFrame = null
  }
}

function cancelTouchZoomAnimation() {
  if (touchZoomAnimationFrame !== null) {
    cancelAnimationFrame(touchZoomAnimationFrame)
    touchZoomAnimationFrame = null
  }

  touchZoomAnimationToken += 1
  zoomRenderToken += 1
  touchZoomTransition = null
  touchZoomRestoreFallback.value = false
  clearThumbnailRestoreStyles()
  clearTouchZoomAnimationStyles()
}

// 清除图片缩放手势的异步帧和基准，切图或组件卸载时不能把旧缩放带到下一张图。
function resetPinchTracking() {
  cancelPanInertia()
  cancelPinchAnimation()
  cancelTouchZoomAnimation()
  pinchActive.value = false
  pinchPointerPair = null
  pinchStartDistance = 0
  pinchStartScale = 1
  pinchAnchorImageX = 0
  pinchAnchorImageY = 0
  pinchFinishing = false
}

// 根据双指当前距离和中心位置，更新缩放比例并保持初始锚点跟随双指中心。
async function applyPinchScale(pair: TouchPointerPair | null) {
  if (pinchFinishing || !pinchActive.value || !displayAsset.value || imageError.value || !pair) {
    return
  }

  const center = getPinchCenter(pair[0], pair[1])
  const viewportPoint = getViewportRelativePoint(center.clientX, center.clientY)
  if (!viewportPoint || pinchStartDistance <= 0) {
    return
  }

  const distance = Math.max(getPinchDistance(pair[0], pair[1]), 1)
  const targetScale = clampActualZoom(pinchStartScale * (distance / pinchStartDistance))
  const renderToken = ++zoomRenderToken
  const anchor: ZoomAnchor = {
    ...viewportPoint,
    imageX: pinchAnchorImageX,
    imageY: pinchAnchorImageY,
  }

  lightbox.setActualZoom(targetScale)
  await restoreZoomAnchor(anchor, targetScale, renderToken)
}

function schedulePinchUpdate() {
  if (pinchFinishing || pinchAnimationFrame !== null) {
    return
  }

  pinchAnimationFrame = requestAnimationFrame(() => {
    pinchAnimationFrame = null
    void applyPinchScale(pinchPointerPair)
  })
}

function beginPinch(pointers: TouchPointerPair) {
  if (pinchFinishing || !displayAsset.value || imageError.value || !hasImageDimensions.value) {
    return
  }

  cancelPanInertia()
  cancelTouchZoomAnimation()
  const center = getPinchCenter(pointers[0], pointers[1])
  const distance = Math.max(getPinchDistance(pointers[0], pointers[1]), 1)
  const startScale = clampActualZoom(getCurrentScale())
  const anchor = getZoomAnchor(center.clientX, center.clientY, startScale)

  zoomRenderToken += 1
  pinchPointerPair = pointers
  pinchFinishing = false
  pinchActive.value = true
  pinchStartDistance = distance
  pinchStartScale = startScale
  pinchAnchorImageX = anchor?.imageX ?? visualImageWidth.value / 2
  pinchAnchorImageY = anchor?.imageY ?? visualImageHeight.value / 2
  // contain 模式切到 actual，但沿用同一个比例，避免双指刚落下时跳变。
  lightbox.setActualZoom(startScale)
}

function updatePinch(pointers: TouchPointerPair) {
  if (!pinchActive.value) {
    return
  }

  pinchPointerPair = [pointers[0], pointers[1]]
  schedulePinchUpdate()
}

// 双指结束后固定最终比例；剩余指针不会自动转成平移，避免手势状态再次分叉。
async function endPinch() {
  if (pinchFinishing) {
    return
  }

  pinchFinishing = true
  cancelPinchAnimation()
  zoomRenderToken += 1

  const finalScale = clampActualZoom(getCurrentScale())
  if (finalScale <= fitScale.value * FIT_MODE_SNAP_RATIO) {
    await showFitMode()
  } else {
    lightbox.setActualZoom(finalScale)
    await nextTick()
    clampViewportScroll()
  }

  pinchActive.value = false
  pinchPointerPair = null
  pinchStartDistance = 0
  pinchFinishing = false
}

// 将内部 viewport 归零；双击过渡完成时保留临时视觉变换直到这里完成。
async function commitFitMode() {
  lightbox.showFitMode()
  await nextTick()
  syncViewportPosition()
}

// 切回适屏模式并把内部 viewport 归零。
async function showFitMode() {
  cancelPanInertia()
  cancelTouchZoomAnimation()
  await commitFitMode()
}

// 以指定屏幕坐标为锚点缩放，保证锚点下的图像内容不跳动。
async function zoomToScaleAtPoint(
  targetScale: number,
  clientX: number,
  clientY: number,
  options: { snapToFit?: boolean } = {}
) {
  cancelPanInertia()
  cancelTouchZoomAnimation()

  if (!displayAsset.value || imageError.value || !hasImageDimensions.value) {
    return
  }

  const snapToFit = options.snapToFit ?? true
  const clampedScale = clampActualZoom(targetScale)

  if (snapToFit && clampedScale <= fitScale.value * FIT_MODE_SNAP_RATIO) {
    // 接近适屏比例时吸附回 contain，避免出现难以察觉的微小放大。
    await showFitMode()
    return
  }

  const anchor = getZoomAnchor(clientX, clientY, getCurrentScale())
  lightbox.setActualZoom(clampedScale)

  if (!anchor) {
    // 没有有效锚点时只需重新布局并居中。
    await nextTick()
    syncViewportPosition()
    return
  }

  await restoreZoomAnchor(anchor, clampedScale)
}

// 在视口中心执行缩放；没有可用 viewport 时退化为直接设置比例。
async function zoomToScaleAtCenter(targetScale: number, options: { snapToFit?: boolean } = {}) {
  cancelPanInertia()
  cancelTouchZoomAnimation()
  const center = getViewportCenterClientPoint()
  if (!center) {
    if ((options.snapToFit ?? true) && targetScale <= fitScale.value * FIT_MODE_SNAP_RATIO) {
      await showFitMode()
      return
    }

    lightbox.setActualZoom(clampActualZoom(targetScale))
    await nextTick()
    syncViewportPosition()
    return
  }

  await zoomToScaleAtPoint(targetScale, center.clientX, center.clientY, options)
}

// 在指定点击位置切换到 1:1。
async function showActualSizeAtPoint(clientX: number, clientY: number) {
  await zoomToScaleAtPoint(1, clientX, clientY, { snapToFit: false })
}

// 计算没有有效锚点时的居中目标；只用于一次性的双击过渡兜底。
function getCenteredStagePosition(scale: number): StagePoint {
  const layout = getZoomLayout(scale)
  return {
    x: VIEWPORT_PADDING + (viewportInnerWidth.value - layout.renderWidth) / 2,
    y: VIEWPORT_PADDING + (viewportInnerHeight.value - layout.renderHeight) / 2,
  }
}

// 触摸双击专用的平滑缩放；桌面点击继续使用上面的即时切换路径。
async function animateTouchZoomAtPoint(clientX: number, clientY: number) {
  if (!displayAsset.value || imageError.value || !hasImageDimensions.value) {
    cancelPanInertia()
    cancelTouchZoomAnimation()
    return
  }

  // 一次只允许一个视觉过渡；新的触摸手势会由 beginPinch/beginPan 负责取消它。
  if (touchZoomTransition) {
    return
  }

  const isFitting = fitMode.value === 'contain'
  const startScale = Math.max(getCurrentScale(), Number.EPSILON)
  const targetScale = Math.max(isFitting ? 1 : fitScale.value, Number.EPSILON)
  const startPosition = getStageViewportPosition()
  if (!startPosition) {
    if (isFitting) {
      lightbox.setActualZoom(targetScale)
      await nextTick()
      syncViewportPosition()
    } else {
      await showFitMode()
    }
    return
  }

  cancelPanInertia()
  cancelTouchZoomAnimation()

  const animationToken = ++touchZoomAnimationToken
  const anchor = getZoomAnchor(clientX, clientY, startScale)
  const targetPosition = anchor
    ? getTouchZoomTargetPosition(anchor, targetScale)
    : getCenteredStagePosition(targetScale)

  if (Math.abs(targetScale - startScale) < 0.001) {
    if (isFitting) {
      lightbox.setActualZoom(targetScale)
      await nextTick()
      if (animationToken === touchZoomAnimationToken) {
        syncViewportPosition()
      }
    } else {
      await showFitMode()
    }
    return
  }

  const transition: TouchZoomTransition = {
    startScale,
    targetScale,
    startPosition,
    targetPosition,
  }
  touchZoomTransition = transition

  let restoreTransition: ThumbnailRestoreTransition | null = null
  if (isFitting) {
    // 放大分支继续使用原有 stage transform，保留原图的渐进式显示效果。
    applyTouchZoomTransform(transition, 0)
  } else {
    // 复原从第一帧开始只显示缩略图；先确认缩略图已解码，避免代理层接管后反而出现空白。
    if (!(await waitForImageReady(thumbnailImageRef.value))) {
      touchZoomTransition = null
      return
    }
    if (animationToken !== touchZoomAnimationToken) {
      return
    }

    touchZoomRestoreFallback.value = true
    originalOpacityTransition.value = false
    // stage 仍保留真实布局，但由独立代理层显示画面，避免 8K stage 被 transform 合成。
    restoreThumbnailActive.value = true
    await nextTick()
    if (animationToken !== touchZoomAnimationToken) {
      return
    }

    const image = restoreThumbnailRef.value
    if (!image) {
      clearThumbnailRestoreStyles()
      touchZoomRestoreFallback.value = false
      touchZoomTransition = null
      return
    }
    if (!(await waitForImageReady(image))) {
      clearThumbnailRestoreStyles()
      touchZoomRestoreFallback.value = false
      touchZoomTransition = null
      return
    }
    if (animationToken !== touchZoomAnimationToken) {
      return
    }

    const thumbnailWidth = image?.naturalWidth ?? 0
    const thumbnailHeight = image?.naturalHeight ?? 0
    if (thumbnailWidth <= 0 || thumbnailHeight <= 0) {
      clearThumbnailRestoreStyles()
      touchZoomRestoreFallback.value = false
      touchZoomTransition = null
      return
    }

    const startLayout = getZoomLayout(startScale)
    const targetLayout = getZoomLayout(targetScale)
    // 代理以自然尺寸渲染，再用 scaleX/scaleY 映射回原图元数据尺寸；这也是 object-fill 的等价处理。
    image.style.width = `${thumbnailWidth}px`
    image.style.height = `${thumbnailHeight}px`
    image.style.transformOrigin = '50% 50%'
    image.style.willChange = 'transform'
    image.style.opacity = '1'
    restoreTransition = {
      startScale,
      targetScale,
      startCenter: getRootRelativePoint({
        x: startPosition.x + startLayout.renderWidth / 2,
        y: startPosition.y + startLayout.renderHeight / 2,
      }),
      targetCenter: getRootRelativePoint({
        x: targetPosition.x + targetLayout.renderWidth / 2,
        y: targetPosition.y + targetLayout.renderHeight / 2,
      }),
      thumbnailWidth,
      thumbnailHeight,
    }
    applyThumbnailRestoreTransform(restoreTransition, 0)
  }

  const startedAt = performance.now()
  const animate = async (now: number) => {
    if (animationToken !== touchZoomAnimationToken) {
      return
    }

    const progress = clamp((now - startedAt) / TOUCH_ZOOM_ANIMATION_DURATION_MS, 0, 1)
    const easedProgress = 1 - Math.pow(1 - progress, 3)
    if (isFitting) {
      applyTouchZoomTransform(transition, easedProgress)
    } else if (restoreTransition) {
      applyThumbnailRestoreTransform(restoreTransition, easedProgress)
    }

    if (progress < 1) {
      touchZoomAnimationFrame = requestAnimationFrame(animate)
      return
    }

    touchZoomAnimationFrame = null
    // 先让最后一帧视觉位置落地，再提交真实布局状态。
    await nextTick()
    if (animationToken !== touchZoomAnimationToken) {
      return
    }

    if (isFitting) {
      // 真实 zoom 更新后，保留 transform 并等待目标布局完成。
      lightbox.setActualZoom(targetScale)
      await nextTick()
      if (animationToken !== touchZoomAnimationToken) {
        return
      }

      // 清掉动画 transform 只用于读取真实 Grid 位置；这一轮 JS 执行结束前不会产生可见帧。
      stageRef.value?.style.removeProperty('transform')
      const contentPosition = getStageContentPosition()
      if (contentPosition) {
        setViewportScroll(
          contentPosition.x - targetPosition.x,
          contentPosition.y - targetPosition.y
        )
      } else {
        syncViewportPosition()
      }
    } else {
      // 缩略图已经覆盖了整个动画；这里继续托底，直到适屏布局和原图 decode 完成。
      const originalReadyPromise = waitForOriginalImageReady()
      await commitFitMode()
      if (animationToken !== touchZoomAnimationToken) {
        return
      }

      // 先恢复适屏 stage 的真实布局，再给普通缩略图一个实际绘制帧，确保它覆盖布局交接过程。
      touchZoomTransition = null
      clearTouchZoomAnimationStyles()
      clearThumbnailRestoreStyles()
      await nextTick()
      await waitForThumbnailPaint()
      if (animationToken !== touchZoomAnimationToken) {
        return
      }

      if (await originalReadyPromise) {
        if (animationToken !== touchZoomAnimationToken) {
          return
        }

        // 原图已可绘制后直接切回，不复用首次加载的淡入过渡。
        originalLoaded.value = true
        originalOpacityTransition.value = false
        touchZoomRestoreFallback.value = false
        await nextTick()
      } else {
        touchZoomRestoreFallback.value = false
      }
    }

    if (animationToken === touchZoomAnimationToken) {
      touchZoomTransition = null
      clearTouchZoomAnimationStyles()
    }
  }

  touchZoomAnimationFrame = requestAnimationFrame(animate)
}

// 在视口中心切换到 1:1。
async function showActualSize() {
  await zoomToScaleAtCenter(1, { snapToFit: false })
}

// 按固定倍率放大当前图片。
async function zoomIn() {
  await zoomToScaleAtCenter(getCurrentScale() * ZOOM_STEP)
}

// 按固定倍率缩小当前图片。
async function zoomOut() {
  await zoomToScaleAtCenter(getCurrentScale() / ZOOM_STEP)
}

// 屏蔽鼠标拖拽后的合成 click，触摸 click 由 Pager 统一拦截。
function scheduleStageClickSuppression(delayMs = 350) {
  suppressStageClick.value = true
  if (suppressStageClickResetTimer !== null) {
    window.clearTimeout(suppressStageClickResetTimer)
  }

  suppressStageClickResetTimer = window.setTimeout(() => {
    suppressStageClick.value = false
    suppressStageClickResetTimer = null
  }, delayMs)
}

function clearStageClickSuppression() {
  suppressStageClick.value = false
  if (suppressStageClickResetTimer !== null) {
    window.clearTimeout(suppressStageClickResetTimer)
    suppressStageClickResetTimer = null
  }
}

// 清除图片内部平移 pointer；触摸 pointer 的捕获由 Pager 统一管理。
function resetPointerState(pointerId?: number) {
  const pointerToRelease = pointerId ?? activePointerId.value
  if (pointerToRelease !== null && stageRef.value?.hasPointerCapture(pointerToRelease)) {
    stageRef.value.releasePointerCapture(pointerToRelease)
  }

  activePointerId.value = null
  dragMoved.value = false
}

// Pager 确认单指已经进入平移后，从这里接管图片 viewport 的滚动基准。
function beginPan(event: PointerEvent) {
  const viewport = viewportRef.value
  if (!displayAsset.value || imageError.value || !isPannable.value || !viewport) {
    return
  }

  cancelPanInertia()
  cancelTouchZoomAnimation()
  resetPointerState()
  activePointerId.value = event.pointerId
  dragStartX.value = event.clientX
  dragStartY.value = event.clientY
  dragStartScrollLeft.value = viewport.scrollLeft
  dragStartScrollTop.value = viewport.scrollTop
  dragMoved.value = false
  panSamples.length = 0
  recordPanSample(event)
}

function movePan(event: PointerEvent): PanMoveResult {
  if (activePointerId.value !== event.pointerId || !viewportRef.value) {
    return { residualX: 0, residualY: 0 }
  }

  const viewport = viewportRef.value
  recordPanSample(event)
  const deltaX = event.clientX - dragStartX.value
  const deltaY = event.clientY - dragStartY.value
  if (!dragMoved.value && Math.hypot(deltaX, deltaY) >= DRAG_THRESHOLD) {
    dragMoved.value = true
  }

  const maxScrollLeft = Math.max(viewport.scrollWidth - viewport.clientWidth, 0)
  const maxScrollTop = Math.max(viewport.scrollHeight - viewport.clientHeight, 0)
  const nextScrollLeft = clamp(dragStartScrollLeft.value - deltaX, 0, maxScrollLeft)
  const nextScrollTop = clamp(dragStartScrollTop.value - deltaY, 0, maxScrollTop)

  setViewportScroll(nextScrollLeft, nextScrollTop)

  // 保留 clamp 丢弃的位移，让外层 Pager 在图片边界把同一次手势接走。
  return {
    residualX: deltaX + (nextScrollLeft - dragStartScrollLeft.value),
    residualY: deltaY + (nextScrollTop - dragStartScrollTop.value),
  }
}

// 正常抬指时根据最近轨迹启动惯性；Pager 负责把取消路径转给 cancelPan。
function endPan(event: PointerEvent) {
  if (activePointerId.value !== event.pointerId) {
    cancelPanInertia()
    resetPointerState()
    return
  }

  recordPanSample(event)
  const velocity = dragMoved.value ? getPanVelocity() : { x: 0, y: 0 }
  resetPointerState(event.pointerId)
  startPanInertia(velocity.x, velocity.y)
}

function cancelPan() {
  cancelPanInertia()
  resetPointerState()
}

// 原图加载完成后再显示原图层，缩略图作为过渡底图保持稳定。
function handleOriginalLoad() {
  originalOpacityTransition.value = true
  originalLoaded.value = true
}

function isRootMappedOriginalUrl(url: string): boolean {
  return /^https:\/\/r-\d+\.test\//i.test(url)
}

// 只对根映射 URL 做一次可达性确认，避免普通资源错误触发无限刷新。
async function tryAutoRecoverByReload() {
  if (autoRecovering.value) {
    return
  }

  const asset = displayAsset.value
  if (!asset) {
    return
  }

  if (!isRootMappedOriginalUrl(originalUrl.value)) {
    return
  }

  const currentUrl = new URL(window.location.href)
  if (currentUrl.searchParams.get('lbRetry') === '1') {
    return
  }

  const reachability = await galleryApi.checkAssetReachable(asset.id)
  if (!reachability.exists || !reachability.readable) {
    return
  }

  autoRecovering.value = true
  currentUrl.searchParams.set('lbAssetId', String(asset.id))
  currentUrl.searchParams.set('lbFolderId', store.filter.folderId ?? 'all')
  currentUrl.searchParams.set('lbRetry', '1')
  window.location.replace(currentUrl.toString())
}

// 原图加载失败时尝试确认文件可达，并通过一次带参数的 reload 恢复映射。
function handleImageError() {
  resetPinchTracking()
  resetPointerState()
  clearStageClickSuppression()
  imageError.value = true

  void tryAutoRecoverByReload().catch((error) => {
    console.warn('Failed to recover lightbox image:', error)
  })
}

// Pager 已经处理触摸轻点；这里仅保留桌面鼠标点击的适屏/实际大小切换。
async function handleStageClick(event: MouseEvent) {
  if (suppressStageClick.value) {
    clearStageClickSuppression()
    return
  }

  if (!displayAsset.value || imageError.value) {
    return
  }

  if (fitMode.value === 'contain') {
    await showActualSizeAtPoint(event.clientX, event.clientY)
    return
  }

  await showFitMode()
}

// 图片平移只保留桌面鼠标路径；触摸平移由 Pager 转发到 beginPan/movePan/endPan。
function handleStagePointerDown(event: PointerEvent) {
  if (isTouchPointerEvent(event)) {
    return
  }

  if (event.button !== 0 || !isPannable.value || !viewportRef.value || !stageRef.value) {
    return
  }

  activePointerId.value = event.pointerId
  dragStartX.value = event.clientX
  dragStartY.value = event.clientY
  dragStartScrollLeft.value = viewportRef.value.scrollLeft
  dragStartScrollTop.value = viewportRef.value.scrollTop
  dragMoved.value = false
  clearStageClickSuppression()
  // 放大图片需要稳定捕获 pointer，避免拖出 stage 后丢失移动事件。
  stageRef.value.setPointerCapture(event.pointerId)
  event.preventDefault()
}

// 将鼠标位移转换成 viewport 的反向滚动。
function handleStagePointerMove(event: PointerEvent) {
  if (isTouchPointerEvent(event)) {
    return
  }

  if (activePointerId.value !== event.pointerId || !viewportRef.value) {
    return
  }

  const deltaX = event.clientX - dragStartX.value
  const deltaY = event.clientY - dragStartY.value

  if (!dragMoved.value && Math.hypot(deltaX, deltaY) >= DRAG_THRESHOLD) {
    dragMoved.value = true
  }

  setViewportScroll(dragStartScrollLeft.value - deltaX, dragStartScrollTop.value - deltaY)
}

// 结束图片平移；真正发生位移时屏蔽后续 click。
function handleStagePointerUp(event: PointerEvent) {
  if (isTouchPointerEvent(event)) {
    return
  }

  if (activePointerId.value !== event.pointerId) {
    return
  }

  if (dragMoved.value) {
    scheduleStageClickSuppression()
  }

  resetPointerState(event.pointerId)
}

// 系统取消图片平移时同样清理 pointer 状态。
function handleStagePointerCancel(event: PointerEvent) {
  if (isTouchPointerEvent(event)) {
    return
  }

  if (activePointerId.value !== event.pointerId) {
    return
  }

  if (dragMoved.value) {
    scheduleStageClickSuppression()
  }

  resetPointerState(event.pointerId)
}

// 图片 stage 丢失捕获时清除本地鼠标拖拽状态。
function handleStageLostPointerCapture(event: PointerEvent) {
  if (isTouchPointerEvent(event)) {
    return
  }

  if (activePointerId.value === event.pointerId) {
    resetPointerState()
  }
}

// 在实际大小或 Ctrl+滚轮时执行以指针为锚点的缩放。
function handleViewportWheel(event: WheelEvent) {
  if (!displayAsset.value || imageError.value) {
    return
  }

  if (!event.ctrlKey && fitMode.value !== 'actual') {
    // 普通适屏滚轮不参与缩放，也不阻止外层滚轮行为。
    return
  }

  event.preventDefault()
  event.stopPropagation()

  if (event.deltaY === 0 || !hasImageDimensions.value || fitScale.value <= 0) {
    return
  }

  const zoomFactor = event.deltaY < 0 ? ZOOM_STEP : 1 / ZOOM_STEP
  void zoomToScaleAtPoint(getCurrentScale() * zoomFactor, event.clientX, event.clientY)
}

defineExpose({
  showFitMode,
  showActualSize,
  animateTouchZoomAtPoint,
  zoomIn,
  zoomOut,
  beginPan,
  movePan,
  endPan,
  cancelPan,
  beginPinch,
  updatePinch,
  endPinch,
})

onUnmounted(() => {
  // 组件卸载后不再执行缩放动画或 click 抑制定时器。
  resetPinchTracking()
  clearStageClickSuppression()
})
</script>

<template>
  <div ref="lightboxRootRef" class="relative h-full w-full overflow-hidden">
    <!-- Pager 仲裁触摸手势；viewport 只负责承载图片滚动和桌面滚轮。 -->
    <div
      ref="viewportRef"
      class="lightbox-viewport absolute inset-0 z-10 h-full w-full overflow-auto"
      @wheel="handleViewportWheel"
    >
      <div class="box-border grid min-h-full min-w-full" :style="canvasStyle">
        <div
          v-if="displayAsset && !imageError"
          ref="stageRef"
          class="relative col-start-1 row-start-1 self-center justify-self-center select-none"
          :style="stageStyle"
          :title="zoomIndicator"
          @click="handleStageClick"
          @pointerdown="handleStagePointerDown"
          @pointermove="handleStagePointerMove"
          @pointerup="handleStagePointerUp"
          @pointercancel="handleStagePointerCancel"
          @lostpointercapture="handleStageLostPointerCapture"
        >
          <!-- 盒子由元数据宽高决定；object-fill 避免缩略图像素比例与元数据不一致时在切换原图时跳动 -->
          <img
            ref="thumbnailImageRef"
            :src="thumbnailUrl"
            :alt="displayAsset.name"
            :style="imageLayerStyle"
            class="absolute top-1/2 left-1/2 max-w-none object-fill select-none"
            draggable="false"
            @dragstart.prevent
          />

          <img
            ref="originalImageRef"
            :src="originalUrl"
            :alt="displayAsset.name"
            :style="originalLayerStyle"
            class="absolute top-1/2 left-1/2 max-w-none object-fill select-none"
            draggable="false"
            @dragstart.prevent
            @load="handleOriginalLoad"
            @error="handleImageError"
          />
        </div>

        <div
          v-else-if="imageError"
          class="col-start-1 row-start-1 flex min-h-full min-w-full flex-col items-center justify-center text-muted-foreground"
        >
          <svg class="mb-4 h-16 w-16" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path
              stroke-linecap="round"
              stroke-linejoin="round"
              stroke-width="2"
              d="M12 8v4m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"
            />
          </svg>
          <p class="text-lg">{{ t('gallery.lightbox.image.loadFailed') }}</p>
          <p class="mt-2 text-sm text-muted-foreground/70">{{ displayAsset?.name }}</p>
        </div>
      </div>
    </div>

    <!-- 仅缩小复原使用；保持缩略图自身尺寸，避免把 8K 图片盒子作为动画图层。 -->
    <img
      v-if="restoreThumbnailActive && displayAsset && !imageError"
      ref="restoreThumbnailRef"
      :src="thumbnailUrl"
      :alt="displayAsset.name"
      class="pointer-events-none absolute top-0 left-0 z-20 max-w-none object-fill select-none"
      style="opacity: 0"
      draggable="false"
      @dragstart.prevent
    />
  </div>
</template>

<style scoped>
.lightbox-viewport {
  /* 触摸手势由外层 Pager 仲裁，图片平移通过代码修改滚动位置。 */
  touch-action: none;
  scrollbar-width: none;
  -ms-overflow-style: none;
}

.lightbox-viewport::-webkit-scrollbar {
  display: none;
}
</style>
