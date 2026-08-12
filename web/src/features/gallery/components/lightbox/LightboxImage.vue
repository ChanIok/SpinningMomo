<script setup lang="ts">
import { computed, nextTick, onUnmounted, ref, watch } from 'vue'
import { useElementSize } from '@vueuse/core'
import { galleryApi } from '../../api'
import { useGalleryData, useGalleryLightbox } from '../../composables'
import { useGalleryStore } from '../../store'
import type { Asset } from '../../types'
import { useI18n } from '@/composables/useI18n'

const VIEWPORT_PADDING = 0
const ZOOM_STEP = 1.1
// 缩放吸附容差：实际缩放比例接近 fitScale 的 1.02 倍以内时，自动吸附回适合模式
const FIT_MODE_SNAP_RATIO = 1.02
const DRAG_THRESHOLD = 4
const MIN_ACTUAL_ZOOM = 0.05
const MAX_ACTUAL_ZOOM = 5

interface ZoomAnchor {
  pointerX: number
  pointerY: number
  imageX: number
  imageY: number
}

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
// 仅在同一张图原图 load 完成淡入时启用；切图隐藏原图时不做过渡，避免叠帧。
const originalOpacityTransition = ref(false)
const autoRecovering = ref(false)
// 实际渲染的资产 id；切图时滞后于 selection，直到目标缩略图 decode 完成再切换。
const displayAssetId = ref<number | null>(null)
let displaySwapToken = 0
const viewportRef = ref<HTMLElement | null>(null)
const stageRef = ref<HTMLElement | null>(null)
const activePointerId = ref<number | null>(null)
const dragStartX = ref(0)
const dragStartY = ref(0)
const dragStartScrollLeft = ref(0)
const dragStartScrollTop = ref(0)
const dragMoved = ref(false)
// 拖拽结束后屏蔽紧随而来的 click 事件，防止误触切换缩放模式
const suppressStageClick = ref(false)
let suppressClickResetTimer: number | null = null

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
  touchAction: isPannable.value ? 'none' : 'auto',
}))

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
  opacity: originalLoaded.value ? 1 : 0,
  transition: originalOpacityTransition.value ? 'opacity 200ms' : 'none',
}))

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
  displayAssetId.value = assetId
  originalLoaded.value = false
  imageError.value = false
  // 切图时清掉旧图的平移和 click 抑制状态，避免把上一张图的交互带过来。
  resetPointerState()
  suppressStageClick.value = false

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
    resetPointerState()
    suppressStageClick.value = false

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

async function restoreZoomAnchor(anchor: ZoomAnchor, scale: number) {
  await nextTick()

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

// 切回适屏模式并把内部 viewport 归零。
async function showFitMode() {
  lightbox.showFitMode()
  await nextTick()
  syncViewportPosition()
}

// 以指定屏幕坐标为锚点缩放，保证锚点下的图像内容不跳动。
async function zoomToScaleAtPoint(
  targetScale: number,
  clientX: number,
  clientY: number,
  options: { snapToFit?: boolean } = {}
) {
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

// 屏蔽图片拖拽后的合成 click，避免误触缩放模式。
function scheduleSuppressClickReset() {
  if (suppressClickResetTimer !== null) {
    window.clearTimeout(suppressClickResetTimer)
  }

  suppressClickResetTimer = window.setTimeout(() => {
    suppressStageClick.value = false
    suppressClickResetTimer = null
  }, 0)
}

// 清除图片内部平移 pointer，并在需要时释放 stage 的捕获。
function resetPointerState(pointerId?: number) {
  if (pointerId !== undefined && stageRef.value?.hasPointerCapture(pointerId)) {
    stageRef.value.releasePointerCapture(pointerId)
  }

  activePointerId.value = null
  dragMoved.value = false
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
  imageError.value = true

  void tryAutoRecoverByReload().catch((error) => {
    console.warn('Failed to recover lightbox image:', error)
  })
}

// 点击图片在适屏和实际大小之间切换；滑动产生的 click 会被前置拦截。
async function handleStageClick(event: MouseEvent) {
  if (suppressStageClick.value) {
    suppressStageClick.value = false
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

// 只有图片处于可平移放大状态时，才由图片自己的拖拽逻辑接管 pointer。
function handleStagePointerDown(event: PointerEvent) {
  if (event.button !== 0 || !isPannable.value || !viewportRef.value || !stageRef.value) {
    return
  }

  activePointerId.value = event.pointerId
  dragStartX.value = event.clientX
  dragStartY.value = event.clientY
  dragStartScrollLeft.value = viewportRef.value.scrollLeft
  dragStartScrollTop.value = viewportRef.value.scrollTop
  dragMoved.value = false
  suppressStageClick.value = false
  // 放大图片需要稳定捕获 pointer，避免拖出 stage 后丢失移动事件。
  stageRef.value.setPointerCapture(event.pointerId)
  event.preventDefault()
}

// 将手指位移转换成 viewport 的反向滚动。
function handleStagePointerMove(event: PointerEvent) {
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
  if (activePointerId.value !== event.pointerId) {
    return
  }

  if (dragMoved.value) {
    suppressStageClick.value = true
    scheduleSuppressClickReset()
  }

  resetPointerState(event.pointerId)
}

// 系统取消图片平移时同样清理 pointer 状态。
function handleStagePointerCancel(event: PointerEvent) {
  if (activePointerId.value !== event.pointerId) {
    return
  }

  if (dragMoved.value) {
    suppressStageClick.value = true
    scheduleSuppressClickReset()
  }

  resetPointerState(event.pointerId)
}

// 图片 stage 丢失捕获时清除本地拖拽状态。
function handleStageLostPointerCapture(event: PointerEvent) {
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
  zoomIn,
  zoomOut,
})

onUnmounted(() => {
  // 组件卸载后不再执行 click 抑制定时器。
  if (suppressClickResetTimer !== null) {
    window.clearTimeout(suppressClickResetTimer)
  }
})
</script>

<template>
  <div class="relative h-full w-full overflow-hidden">
    <div
      ref="viewportRef"
      class="lightbox-viewport absolute inset-0 z-10 h-full w-full overflow-auto"
      :style="{ touchAction: isPannable ? 'none' : 'pan-y' }"
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
            :src="thumbnailUrl"
            :alt="displayAsset.name"
            :style="imageLayerStyle"
            class="absolute top-1/2 left-1/2 max-w-none object-fill select-none"
            draggable="false"
            @dragstart.prevent
          />

          <img
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
  </div>
</template>

<style scoped>
.lightbox-viewport {
  scrollbar-width: none;
  -ms-overflow-style: none;
}

.lightbox-viewport::-webkit-scrollbar {
  display: none;
}
</style>
