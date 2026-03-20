<script setup lang="ts">
/**
 * 灯箱内视频：原片走 galleryApi.getAssetUrl（后端 Range），poster 为图库 WebP 缩略图。
 * :key="currentAsset.id" 在切条目时强制重建 <video>，避免沿用上一段的 currentTime/缓冲。
 */
import { computed } from 'vue'
import { galleryApi } from '../../api'
import { useGalleryStore } from '../../store'
import { useI18n } from '@/composables/useI18n'

const emit = defineEmits<{
  previous: []
  next: []
}>()

const store = useGalleryStore()
const { t } = useI18n()

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

  return galleryApi.getAssetUrl(currentAsset.value.id)
})

const posterUrl = computed(() => {
  if (!currentAsset.value) {
    return ''
  }

  return galleryApi.getAssetThumbnailUrl(currentAsset.value)
})

const canGoToPrevious = computed(() => (store.selection.activeIndex ?? 0) > 0)
const canGoToNext = computed(() => (store.selection.activeIndex ?? 0) < store.totalCount - 1)

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

    <div class="flex h-full w-full items-center justify-center p-8">
      <video
        v-if="currentAsset"
        :key="currentAsset.id"
        :src="assetUrl"
        :poster="posterUrl"
        :aria-label="currentAsset.name"
        class="max-h-full max-w-full rounded-lg shadow-2xl"
        autoplay
        controls
        playsinline
        preload="metadata"
      />
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
