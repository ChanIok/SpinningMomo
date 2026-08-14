<script setup lang="ts">
import { computed, ref } from 'vue'
import { useElementSize } from '@vueuse/core'
import { galleryApi } from '../../api'
import { useGalleryLightbox, useLightboxSwipeNavigation } from '../../composables'
import { useGalleryStore } from '../../store'
import { heroAnimating } from '../../composables/useHeroTransition'
import LightboxImage from './LightboxImage.vue'
import LightboxVideo from './LightboxVideo.vue'

interface LightboxImageExposed {
  showFitMode: () => void
  showActualSize: () => void
  animateTouchZoomAtPoint: (clientX: number, clientY: number) => Promise<void>
  zoomIn: () => void
  zoomOut: () => void
  beginPan: (event: PointerEvent) => void
  movePan: (event: PointerEvent) => void
  endPan: (event: PointerEvent) => void
  cancelPan: () => void
  beginPinch: (pointers: readonly [TouchPointer, TouchPointer]) => void
  updatePinch: (pointers: readonly [TouchPointer, TouchPointer]) => void
  endPinch: () => void
}

type TouchPointer = Pick<PointerEvent, 'pointerId' | 'clientX' | 'clientY'>

const TOUCH_DOUBLE_TAP_WINDOW_MS = 300

const store = useGalleryStore()
const lightbox = useGalleryLightbox()
const pagerRef = ref<HTMLElement | null>(null)
const imageRef = ref<LightboxImageExposed | null>(null)
const imagePannable = ref(false)
const { width } = useElementSize(pagerRef)

const emit = defineEmits<{
  'touch-tap': [isDoubleTap: boolean]
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
// 视频始终允许切图；放大图片时由同一个 Pager 把单指移动转发给 Image 平移。
const canSwipeGesture = computed(
  () => !heroAnimating.value && (isVideo.value || (isStillImage.value && !imagePannable.value))
)

function resetTouchTapTracking() {
  lastTouchTapAt = Number.NEGATIVE_INFINITY
  lastTouchTapAssetId = null
}

function handleTouchTap(event: PointerEvent, startTarget: EventTarget | null): boolean {
  if (
    isVideo.value &&
    (startTarget instanceof HTMLVideoElement || event.target instanceof HTMLVideoElement)
  ) {
    // 视频控件的触摸 click 仍交给原生播放器，不切换图库 chrome。
    resetTouchTapTracking()
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
  enabled: canSwipeGesture,
  pannable: computed(() => !heroAnimating.value && imagePannable.value),
  navigateToIndex: (index) => lightbox.goToIndex(index),
  onTouchTap: handleTouchTap,
  onPanStart: (event) => {
    resetTouchTapTracking()
    imageRef.value?.beginPan(event)
  },
  onPanMove: (event) => {
    imageRef.value?.movePan(event)
  },
  onPanEnd: (event) => {
    imageRef.value?.endPan(event)
  },
  onPanCancel: () => {
    imageRef.value?.cancelPan()
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

// 图片放大到需要拖拽时，Pager 暂停自己的横向手势。
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
      <LightboxVideo v-else @ready="handleMediaReady" />
    </div>
  </div>
</template>
