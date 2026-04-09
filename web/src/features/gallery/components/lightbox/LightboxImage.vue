<script setup lang="ts">
import { computed, nextTick, ref, watch } from 'vue'
import { useElementSize } from '@vueuse/core'
import { galleryApi } from '../../api'
import { useGalleryData, useGalleryLightbox } from '../../composables'
import { useGalleryStore } from '../../store'
import { useI18n } from '@/composables/useI18n'
import { heroAnimating } from '../../composables/useHeroTransition'

const VIEWPORT_PADDING = 32
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

const imageError = ref(false)
const originalLoaded = ref(false)
const switchingFrame = ref(false)
const previousFrame = ref<{
  id: number
  name: string
  thumbnailUrl: string
} | null>(null)
const viewportRef = ref<HTMLElement | null>(null)
const stageRef = ref<HTMLElement | null>(null)
const activePointerId = ref<number | null>(null)
const dragStartX = ref(0)
const dragStartY = ref(0)
const dragStartScrollLeft = ref(0)
const dragStartScrollTop = ref(0)
const dragMoved = ref(false)
// 拖拽结束后屏蔽紧随而来的 click 事件，防止误触切换缩放模式
const suppressClick = ref(false)
let suppressClickResetTimer: number | null = null

const { width, height } = useElementSize(viewportRef)

const availableWidth = computed(() => width.value)
const availableHeight = computed(() => height.value)
const viewportInnerWidth = computed(() => Math.max(availableWidth.value - VIEWPORT_PADDING * 2, 1))
const viewportInnerHeight = computed(() =>
  Math.max(availableHeight.value - VIEWPORT_PADDING * 2, 1)
)

const currentAsset = computed(() => {
  const currentIdx = store.selection.activeIndex
  if (currentIdx === undefined) {
    return null
  }

  return store.getAssetsInRange(currentIdx, currentIdx)[0]
})

const thumbnailUrl = computed(() => {
  if (!currentAsset.value) return ''
  return galleryApi.getAssetThumbnailUrl(currentAsset.value)
})

const originalUrl = computed(() => {
  if (!currentAsset.value) return ''
  return galleryData.getAssetUrl(currentAsset.value)
})

const canGoToPrevious = computed(() => (store.selection.activeIndex ?? 0) > 0)
const canGoToNext = computed(() => (store.selection.activeIndex ?? 0) < store.totalCount - 1)
const fitMode = computed(() => store.lightbox.fitMode)
const actualZoom = computed(() => store.lightbox.zoom)

const imageWidth = computed(() => currentAsset.value?.width || 0)
const imageHeight = computed(() => currentAsset.value?.height || 0)
const hasImageDimensions = computed(() => imageWidth.value > 0 && imageHeight.value > 0)

const fitScale = computed(() => {
  if (
    !hasImageDimensions.value ||
    viewportInnerWidth.value <= 0 ||
    viewportInnerHeight.value <= 0
  ) {
    return 1
  }

  return Math.min(
    viewportInnerWidth.value / imageWidth.value,
    viewportInnerHeight.value / imageHeight.value,
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

  return Math.max(imageWidth.value * displayScale.value, 1)
})

const renderHeight = computed(() => {
  if (!hasImageDimensions.value) {
    return Math.max(viewportInnerHeight.value, 1)
  }

  return Math.max(imageHeight.value * displayScale.value, 1)
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

const isDragging = computed(() => activePointerId.value !== null)

const stageCursor = computed(() => {
  if (!currentAsset.value || imageError.value) {
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

const zoomIndicator = computed(() => {
  if (fitMode.value === 'contain') {
    return t('gallery.lightbox.image.fitIndicator', { percent: Math.round(fitScale.value * 100) })
  }

  return `${Math.round(actualZoom.value * 100)}%`
})

function finishFrameSwitch() {
  if (!switchingFrame.value) {
    return
  }

  switchingFrame.value = false
  previousFrame.value = null
}

watch(
  () => currentAsset.value,
  async (newAsset, oldAsset) => {
    if (oldAsset && newAsset && oldAsset.id !== newAsset.id && !imageError.value) {
      previousFrame.value = {
        id: oldAsset.id,
        name: oldAsset.name,
        thumbnailUrl: galleryApi.getAssetThumbnailUrl(oldAsset),
      }
      switchingFrame.value = true
    } else if (!newAsset || !oldAsset) {
      previousFrame.value = null
      switchingFrame.value = false
    }

    originalLoaded.value = false
    imageError.value = false
    resetPointerState()
    suppressClick.value = false

    await nextTick()
    syncViewportPosition()
  },
  { immediate: true, flush: 'post' }
)

watch(
  [availableWidth, availableHeight],
  async () => {
    await nextTick()
    if (fitMode.value === 'contain') {
      syncViewportPosition()
      return
    }

    clampViewportScroll()
  },
  { flush: 'post' }
)

function clamp(value: number, min: number, max: number): number {
  return Math.min(Math.max(value, min), max)
}

function clampActualZoom(zoom: number): number {
  return clamp(zoom, MIN_ACTUAL_ZOOM, MAX_ACTUAL_ZOOM)
}

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

function setViewportScroll(left: number, top: number) {
  const viewport = viewportRef.value
  if (!viewport) return

  viewport.scrollLeft = clamp(left, 0, Math.max(viewport.scrollWidth - viewport.clientWidth, 0))
  viewport.scrollTop = clamp(top, 0, Math.max(viewport.scrollHeight - viewport.clientHeight, 0))
}

function getCurrentScale(): number {
  return fitMode.value === 'contain' ? fitScale.value : actualZoom.value
}

function syncViewportPosition() {
  const viewport = viewportRef.value
  if (!viewport) return

  if (fitMode.value === 'contain') {
    viewport.scrollLeft = 0
    viewport.scrollTop = 0
    return
  }

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
    imageX: clamp((viewport.scrollLeft + pointerX - stageOffsetLeft) / scale, 0, imageWidth.value),
    imageY: clamp((viewport.scrollTop + pointerY - stageOffsetTop) / scale, 0, imageHeight.value),
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

async function showFitMode() {
  lightbox.showFitMode()
  await nextTick()
  syncViewportPosition()
}

async function zoomToScaleAtPoint(
  targetScale: number,
  clientX: number,
  clientY: number,
  options: { snapToFit?: boolean } = {}
) {
  if (!currentAsset.value || imageError.value || !hasImageDimensions.value) {
    return
  }

  const snapToFit = options.snapToFit ?? true
  const clampedScale = clampActualZoom(targetScale)

  if (snapToFit && clampedScale <= fitScale.value * FIT_MODE_SNAP_RATIO) {
    await showFitMode()
    return
  }

  const anchor = getZoomAnchor(clientX, clientY, getCurrentScale())
  lightbox.setActualZoom(clampedScale)

  if (!anchor) {
    await nextTick()
    syncViewportPosition()
    return
  }

  await restoreZoomAnchor(anchor, clampedScale)
}

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

async function showActualSizeAtPoint(clientX: number, clientY: number) {
  await zoomToScaleAtPoint(1, clientX, clientY, { snapToFit: false })
}

async function showActualSize() {
  await zoomToScaleAtCenter(1, { snapToFit: false })
}

async function zoomIn() {
  await zoomToScaleAtCenter(getCurrentScale() * ZOOM_STEP)
}

async function zoomOut() {
  await zoomToScaleAtCenter(getCurrentScale() / ZOOM_STEP)
}

function scheduleSuppressClickReset() {
  if (suppressClickResetTimer !== null) {
    window.clearTimeout(suppressClickResetTimer)
  }

  suppressClickResetTimer = window.setTimeout(() => {
    suppressClick.value = false
    suppressClickResetTimer = null
  }, 0)
}

function resetPointerState(pointerId?: number) {
  if (pointerId !== undefined && stageRef.value?.hasPointerCapture(pointerId)) {
    stageRef.value.releasePointerCapture(pointerId)
  }

  activePointerId.value = null
  dragMoved.value = false
}

function handleOriginalLoad() {
  originalLoaded.value = true
  finishFrameSwitch()
}

function handleThumbnailLoad() {
  finishFrameSwitch()
}

function handleImageError() {
  imageError.value = true
  finishFrameSwitch()
}

function handlePrevious() {
  lightbox.goToPrevious()
}

function handleNext() {
  lightbox.goToNext()
}

async function handleStageClick(event: MouseEvent) {
  if (suppressClick.value) {
    suppressClick.value = false
    return
  }

  if (!currentAsset.value || imageError.value) {
    return
  }

  if (fitMode.value === 'contain') {
    await showActualSizeAtPoint(event.clientX, event.clientY)
    return
  }

  await showFitMode()
}

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
  suppressClick.value = false
  stageRef.value.setPointerCapture(event.pointerId)
  event.preventDefault()
}

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

function handleStagePointerUp(event: PointerEvent) {
  if (activePointerId.value !== event.pointerId) {
    return
  }

  if (dragMoved.value) {
    suppressClick.value = true
    scheduleSuppressClickReset()
  }

  resetPointerState(event.pointerId)
}

function handleStagePointerCancel(event: PointerEvent) {
  if (activePointerId.value !== event.pointerId) {
    return
  }

  if (dragMoved.value) {
    suppressClick.value = true
    scheduleSuppressClickReset()
  }

  resetPointerState(event.pointerId)
}

function handleStageLostPointerCapture(event: PointerEvent) {
  if (activePointerId.value === event.pointerId) {
    resetPointerState()
  }
}

function handleViewportWheel(event: WheelEvent) {
  if (!currentAsset.value || imageError.value) {
    return
  }

  if (!event.ctrlKey && fitMode.value !== 'actual') {
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
</script>

<template>
  <div class="relative h-full w-full">
    <button
      v-if="canGoToPrevious"
      class="surface-top absolute top-1/2 left-4 z-10 inline-flex h-12 w-12 -translate-y-1/2 items-center justify-center rounded-full text-foreground transition-all"
      @click="handlePrevious"
      :title="t('gallery.lightbox.image.previousTitle')"
    >
      <svg class="h-6 w-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 19l-7-7 7-7" />
      </svg>
    </button>

    <div
      ref="viewportRef"
      class="lightbox-viewport h-full w-full overflow-auto"
      :style="heroAnimating ? { visibility: 'hidden' } : {}"
      @wheel="handleViewportWheel"
    >
      <div class="box-border grid min-h-full min-w-full" :style="canvasStyle">
        <div
          v-if="currentAsset && !imageError"
          :key="currentAsset.id"
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
          <img
            v-if="switchingFrame && previousFrame"
            :src="previousFrame.thumbnailUrl"
            :alt="previousFrame.name"
            class="absolute inset-0 h-full w-full object-contain select-none"
            draggable="false"
            @dragstart.prevent
          />

          <img
            :src="thumbnailUrl"
            :alt="currentAsset.name"
            class="absolute inset-0 h-full w-full object-contain select-none"
            draggable="false"
            @dragstart.prevent
            @load="handleThumbnailLoad"
          />

          <img
            :src="originalUrl"
            :alt="currentAsset.name"
            :style="{
              opacity: originalLoaded ? 1 : 0,
            }"
            class="absolute inset-0 h-full w-full object-contain transition-opacity duration-200 select-none"
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
          <p class="mt-2 text-sm text-muted-foreground/70">{{ currentAsset?.name }}</p>
        </div>
      </div>
    </div>

    <button
      v-if="canGoToNext"
      class="surface-top absolute top-1/2 right-4 z-10 inline-flex h-12 w-12 -translate-y-1/2 items-center justify-center rounded-full text-foreground transition-all"
      @click="handleNext"
      :title="t('gallery.lightbox.image.nextTitle')"
    >
      <svg class="h-6 w-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 5l7 7-7 7" />
      </svg>
    </button>
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
