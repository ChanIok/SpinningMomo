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
  zoomIn: () => void
  zoomOut: () => void
}

const store = useGalleryStore()
const lightbox = useGalleryLightbox()
const pagerRef = ref<HTMLElement | null>(null)
const imageRef = ref<LightboxImageExposed | null>(null)
const imagePannable = ref(false)
const { width } = useElementSize(pagerRef)

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
// 视频始终允许切图；图片只有不需要内部平移时才把 pointer 交给 Pager。
const canSwipeGesture = computed(
  () => !heroAnimating.value && (isVideo.value || (isStillImage.value && !imagePannable.value))
)

const {
  swipePhase,
  swipePreviewPages,
  swipeViewportStyle,
  swipeGestureSurfaceStyle,
  consumeSuppressedClick,
  completeNavigation,
  handleSwipePointerDown,
  handleSwipePointerMove,
  handleSwipePointerUp,
  handleSwipePointerCancel,
  handleSwipeLostPointerCapture,
} = useLightboxSwipeNavigation({
  gestureSurfaceRef: pagerRef,
  availableWidth: width,
  enabled: canSwipeGesture,
  navigateToIndex: (index) => lightbox.goToIndex(index),
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
  if (swipePhase.value !== 'idle' || consumeSuppressedClick()) {
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
    @pointerdown="handleSwipePointerDown"
    @pointermove="handleSwipePointerMove"
    @pointerup="handleSwipePointerUp"
    @pointercancel="handleSwipePointerCancel"
    @lostpointercapture="handleSwipeLostPointerCapture"
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
