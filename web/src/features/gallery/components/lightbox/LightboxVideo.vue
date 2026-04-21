<script setup lang="ts">
/**
 * 灯箱内视频：原片走 galleryApi.getAssetUrl（后端 Range），poster 为图库 WebP 缩略图。
 * :key="currentAsset.id" 在切条目时强制重建 <video>，避免沿用上一段的 currentTime/缓冲。
 */
import { computed, ref, watch } from 'vue'
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

watch(
  () => currentAsset.value?.id,
  () => {
    videoError.value = false
    autoRecovering.value = false
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

function handlePrevious() {
  emit('previous')
}

function handleNext() {
  emit('next')
}
</script>

<template>
  <div class="relative flex h-full w-full items-center justify-center">
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
      class="flex h-full w-full items-center justify-center p-8"
      :style="heroAnimating ? { visibility: 'hidden' } : {}"
    >
      <video
        v-if="currentAsset && !videoError"
        :key="currentAsset.id"
        :src="assetUrl"
        :poster="posterUrl"
        :aria-label="currentAsset.name"
        class="max-h-full max-w-full rounded-lg shadow-2xl"
        autoplay
        controls
        playsinline
        preload="metadata"
        @error="handleVideoError"
      />

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
