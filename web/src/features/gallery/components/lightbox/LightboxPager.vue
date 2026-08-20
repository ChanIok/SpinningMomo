<script setup lang="ts">
import { computed, ref } from 'vue'
import { useElementSize } from '@vueuse/core'
import { galleryApi } from '../../api'
import { useGalleryLightbox, useLightboxSwipeNavigation } from '../../composables'
import type { LightboxVerticalGestureAction } from '../../composables/useLightboxSwipeNavigation'
import { useGalleryStore } from '../../store'
import { heroAnimating } from '../../composables/useHeroTransition'
import LightboxImage from './LightboxImage.vue'
import LightboxVideo from './LightboxVideo.vue'

interface PanMoveResult {
  residualX: number
  residualY: number
}

interface LightboxImageExposed {
  showFitMode: () => void
  showActualSize: () => void
  animateTouchZoomAtPoint: (clientX: number, clientY: number) => Promise<void>
  zoomIn: () => void
  zoomOut: () => void
  beginPan: (event: PointerEvent) => void
  movePan: (event: PointerEvent) => PanMoveResult
  endPan: (event: PointerEvent) => void
  cancelPan: () => void
  beginPinch: (pointers: readonly [TouchPointer, TouchPointer]) => void
  updatePinch: (pointers: readonly [TouchPointer, TouchPointer]) => void
  endPinch: () => void
}

interface LightboxVideoExposed {
  isInNativeControlZone: (clientX: number, clientY: number) => boolean
}

type TouchPointer = Pick<PointerEvent, 'pointerId' | 'clientX' | 'clientY'>

const TOUCH_DOUBLE_TAP_WINDOW_MS = 300

const props = defineProps<{
  verticalGestureEnabled: boolean
  touchChromeEnabled: boolean
}>()

const store = useGalleryStore()
const lightbox = useGalleryLightbox()
const pagerRef = ref<HTMLElement | null>(null)
const imageRef = ref<LightboxImageExposed | null>(null)
const videoRef = ref<LightboxVideoExposed | null>(null)
const imagePannable = ref(false)
const { width, height } = useElementSize(pagerRef)

const emit = defineEmits<{
  'touch-tap': [isDoubleTap: boolean]
  'vertical-gesture-move': [offsetY: number, progress: number]
  'vertical-gesture-cancel': [offsetY: number]
  'vertical-gesture-commit': [action: LightboxVerticalGestureAction, offsetY: number]
}>()

let lastTouchTapAt = Number.NEGATIVE_INFINITY
let lastTouchTapAssetId: number | null = null

// Pager 只读取 Pinia 当前索引，具体媒体由下面的 Image/Video 渲染器决定。
const currentAsset = computed(() => {
  const activeIndex = store.selection.activeIndex
  if (activeIndex === undefined) {
    return null
  }

  return store.getAssetsInRange(activeIndex, activeIndex)[0] ?? null
})

const isVideo = computed(() => currentAsset.value?.type === 'video')
const isStillImage = computed(
  () => currentAsset.value?.type === 'photo' || currentAsset.value?.type === 'live_photo'
)
// 视频始终允许切图；放大图片时先由 Image 消费平移，触及水平边界后再交给 Pager 切图。
const canSwipeGesture = computed(
  () => !heroAnimating.value && (isVideo.value || (isStillImage.value && !imagePannable.value))
)
const canUseVerticalGesture = computed(
  () => props.verticalGestureEnabled && !heroAnimating.value && !imagePannable.value
)

function canStartVerticalGesture(target: EventTarget | null): boolean {
  if (!(target instanceof HTMLElement)) {
    return true
  }

  // 明确的控件必须保留原生点击/拖动语义；媒体画面本身参与暗房纵向导航。
  return !target.closest('button, a, input, textarea, select, [role="button"], [role="slider"]')
}

function isInNativeVideoControlZone(event: PointerEvent): boolean {
  return (
    isVideo.value && videoRef.value?.isInNativeControlZone(event.clientX, event.clientY) === true
  )
}

// 原生视频控制条所在的安全带不应被 Pager 接管，否则播放/进度拖动会变成切图或暗房手势。
function canStartPagerGesture(event: PointerEvent): boolean {
  return !isInNativeVideoControlZone(event)
}

function resetTouchTapTracking() {
  lastTouchTapAt = Number.NEGATIVE_INFINITY
  lastTouchTapAssetId = null
}

function handleTouchTap(event: PointerEvent, _startTarget: EventTarget | null): boolean {
  if (isVideo.value) {
    resetTouchTapTracking()
    if (!isInNativeVideoControlZone(event)) {
      // 视频画面单击切换图库 chrome，但不抑制原生 click，让正中播放按钮仍可工作。
      emit('touch-tap', false)
    }
    return false
  }

  const assetId = currentAsset.value?.id ?? null
  const now = performance.now()
  const isDoubleTap =
    isStillImage.value &&
    assetId !== null &&
    lastTouchTapAssetId === assetId &&
    now - lastTouchTapAt <= TOUCH_DOUBLE_TAP_WINDOW_MS

  if (isDoubleTap) {
    resetTouchTapTracking()
    void imageRef.value?.animateTouchZoomAtPoint(event.clientX, event.clientY)
  } else if (isStillImage.value && assetId !== null) {
    lastTouchTapAt = now
    lastTouchTapAssetId = assetId
  } else {
    resetTouchTapTracking()
  }

  emit('touch-tap', isDoubleTap)
  return true
}

const {
  swipePhase,
  multiTouchActive,
  swipePreviewPages,
  swipeViewportStyle,
  swipeGestureSurfaceStyle,
  consumeSuppressedClick,
  completeNavigation,
  handleSwipePointerDown,
  handleSwipePointerMove,
  handleSwipePointerUp,
  handleSwipePointerCancel,
} = useLightboxSwipeNavigation({
  gestureSurfaceRef: pagerRef,
  availableWidth: width,
  availableHeight: height,
  enabled: canSwipeGesture,
  pannable: computed(() => !heroAnimating.value && imagePannable.value),
  verticalGestureEnabled: canUseVerticalGesture,
  navigateToIndex: (index) => lightbox.goToIndex(index),
  canStartGesture: canStartPagerGesture,
  canStartVerticalGesture,
  onTouchTap: handleTouchTap,
  onPanStart: (event) => {
    resetTouchTapTracking()
    imageRef.value?.beginPan(event)
  },
  onPanMove: (event) => imageRef.value?.movePan(event),
  onPanEnd: (event) => {
    imageRef.value?.endPan(event)
  },
  onPanCancel: () => {
    imageRef.value?.cancelPan()
  },
  onVerticalGestureMove: (offsetY, progress) => {
    emit('vertical-gesture-move', offsetY, progress)
  },
  onVerticalGestureCancel: (offsetY) => {
    emit('vertical-gesture-cancel', offsetY)
  },
  onVerticalGestureCommit: (action, offsetY) => {
    emit('vertical-gesture-commit', action, offsetY)
  },
  onPinchStart: (pointers) => {
    resetTouchTapTracking()
    imageRef.value?.beginPinch(pointers)
  },
  onPinchMove: (pointers) => {
    imageRef.value?.updatePinch(pointers)
  },
  onPinchEnd: () => {
    imageRef.value?.endPinch()
  },
})

// 媒体完成挂载后，通知导航状态机把目标页提升为新的基准页。
function handleMediaReady(assetId: number) {
  completeNavigation(assetId)
}

// 图片放大到需要拖拽时，Pager 先转发平移；水平边界转交由手势状态机继续处理。
function handleImagePannableChange(pannable: boolean) {
  imagePannable.value = pannable
}

// 滑动产生的 click 不能落到图片缩放或视频控件上。
function handlePagerClickCapture(event: MouseEvent) {
  if (swipePhase.value !== 'idle' || multiTouchActive.value || consumeSuppressedClick()) {
    event.preventDefault()
    event.stopPropagation()
  }
}

// 对外保持原有灯箱工具栏的缩放接口，实际调用转发给图片渲染器。
defineExpose({
  showFitMode: async () => {
    await imageRef.value?.showFitMode()
  },
  showActualSize: async () => {
    await imageRef.value?.showActualSize()
  },
  zoomIn: async () => {
    await imageRef.value?.zoomIn()
  },
  zoomOut: async () => {
    await imageRef.value?.zoomOut()
  },
})
</script>

<template>
  <div
    ref="pagerRef"
    class="relative h-full w-full overflow-hidden"
    :style="swipeGestureSurfaceStyle"
    @click.capture="handlePagerClickCapture"
    @pointerdown.capture="handleSwipePointerDown"
    @pointermove.capture="handleSwipePointerMove"
    @pointerup.capture="handleSwipePointerUp"
    @pointercancel.capture="handleSwipePointerCancel"
  >
    <!-- 相邻页只渲染缩略图；当前页由独立的图片/视频渲染器负责。 -->
    <div
      v-for="page in swipePreviewPages"
      :key="page.asset.id"
      class="pointer-events-none absolute inset-0 z-0 overflow-hidden bg-transparent"
      :style="page.style"
    >
      <img
        :src="galleryApi.getAssetThumbnailUrl(page.asset)"
        :alt="page.asset.name"
        class="h-full w-full object-contain select-none"
        draggable="false"
      />
    </div>

    <!-- 当前媒体页使用轨道偏移移动，ready 后才会被重新放回中心。 -->
    <div
      class="absolute inset-0 z-10"
      :style="[swipeViewportStyle, heroAnimating ? { visibility: 'hidden' } : {}]"
    >
      <LightboxImage
        v-if="!isVideo"
        ref="imageRef"
        @ready="handleMediaReady"
        @pannable-change="handleImagePannableChange"
      />
      <LightboxVideo
        v-else
        ref="videoRef"
        :touch-chrome-enabled="props.touchChromeEnabled"
        @ready="handleMediaReady"
      />
    </div>
  </div>
</template>
