<script setup lang="ts">
/**
 * 灯箱内视频：原片走 galleryApi.getAssetUrl（后端 Range），poster 为图库 WebP 缩略图。
 * :key="currentAsset.id" 在切条目时强制重建 <video>，避免沿用上一段的 currentTime/缓冲。
 */
import { computed, ref, watch } from 'vue'
import { useElementSize } from '@vueuse/core'
import { galleryApi } from '../../api'
import { useGalleryStore } from '../../store'
import { useI18n } from '@/composables/useI18n'
import { heroAnimating } from '../../composables/useHeroTransition'

const emit = defineEmits<{
  previous: []
  next: []
}>()

const store = useGalleryStore()
const { t } = useI18n()
const videoError = ref(false)
const autoRecovering = ref(false)
const videoReady = ref(false)
const viewportRef = ref<HTMLElement | null>(null)

const VIEWPORT_PADDING = 32

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

  return store.getAssetsInRange(currentIdx, currentIdx)[0] ?? null
})

const assetUrl = computed(() => {
  if (!currentAsset.value) {
    return ''
  }

  return galleryApi.getAssetUrl(currentAsset.value)
})

const posterUrl = computed(() => {
  if (!currentAsset.value) {
    return ''
  }

  return galleryApi.getAssetThumbnailUrl(currentAsset.value)
})

const canGoToPrevious = computed(() => (store.selection.activeIndex ?? 0) > 0)
const canGoToNext = computed(() => (store.selection.activeIndex ?? 0) < store.totalCount - 1)
const mediaWidth = computed(() => currentAsset.value?.width || 0)
const mediaHeight = computed(() => currentAsset.value?.height || 0)
const hasMediaDimensions = computed(() => mediaWidth.value > 0 && mediaHeight.value > 0)

const fitScale = computed(() => {
  if (
    !hasMediaDimensions.value ||
    viewportInnerWidth.value <= 0 ||
    viewportInnerHeight.value <= 0
  ) {
    return 1
  }

  return Math.min(
    viewportInnerWidth.value / mediaWidth.value,
    viewportInnerHeight.value / mediaHeight.value,
    1
  )
})

const renderWidth = computed(() => {
  if (!hasMediaDimensions.value) {
    return Math.max(viewportInnerWidth.value, 1)
  }

  return Math.max(mediaWidth.value * fitScale.value, 1)
})

const renderHeight = computed(() => {
  if (!hasMediaDimensions.value) {
    return Math.max(viewportInnerHeight.value, 1)
  }

  return Math.max(mediaHeight.value * fitScale.value, 1)
})

const stageStyle = computed(() => ({
  width: `${renderWidth.value}px`,
  height: `${renderHeight.value}px`,
}))

watch(
  () => currentAsset.value?.id,
  () => {
    videoError.value = false
    autoRecovering.value = false
    videoReady.value = false
  },
  { immediate: true }
)

function isRootMappedOriginalUrl(url: string): boolean {
  return /^https:\/\/r-\d+\.test\//i.test(url)
}

async function tryAutoRecoverByReload() {
  if (autoRecovering.value) {
    return
  }

  const asset = currentAsset.value
  if (!asset) {
    return
  }

  if (!isRootMappedOriginalUrl(assetUrl.value)) {
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

function handleVideoError() {
  videoError.value = true
  void tryAutoRecoverByReload().catch((error) => {
    console.warn('Failed to recover lightbox video:', error)
  })
}

function handleVideoLoadedData() {
  videoReady.value = true
}

function handlePrevious() {
  emit('previous')
}

function handleNext() {
  emit('next')
}
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
      class="h-full w-full p-8"
      :style="heroAnimating ? { visibility: 'hidden' } : {}"
    >
      <div class="flex h-full w-full items-center justify-center">
        <div
          v-if="currentAsset && !videoError"
          :key="currentAsset.id"
          class="relative overflow-hidden rounded-lg shadow-2xl"
          :style="stageStyle"
        >
          <img
            :src="posterUrl"
            :alt="currentAsset.name"
            class="absolute inset-0 h-full w-full object-contain select-none"
            draggable="false"
            @dragstart.prevent
          />

          <video
            :src="assetUrl"
            :poster="posterUrl"
            :aria-label="currentAsset.name"
            :style="{ opacity: videoReady ? 1 : 0 }"
            class="absolute inset-0 h-full w-full object-contain transition-opacity duration-200"
            autoplay
            controls
            playsinline
            preload="metadata"
            @loadeddata="handleVideoLoadedData"
            @error="handleVideoError"
          />
        </div>

        <div
          v-else-if="videoError"
          class="flex min-h-full min-w-full flex-col items-center justify-center text-muted-foreground"
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
