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

const store = useGalleryStore()
const { t } = useI18n()
const emit = defineEmits<{
  ready: [assetId: number]
}>()
const videoError = ref(false)
const autoRecovering = ref(false)
const videoReady = ref(false)
const viewportRef = ref<HTMLElement | null>(null)

const VIEWPORT_PADDING = 0

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

// activeIndex 改变时重置播放器状态，并通知 Pager 视频页面已经可以接管中心位置。
watch(
  () => currentAsset.value?.id,
  (assetId) => {
    videoError.value = false
    autoRecovering.value = false
    videoReady.value = false
    if (assetId !== undefined) {
      emit('ready', assetId)
    }
  },
  { immediate: true }
)

function isRootMappedOriginalUrl(url: string): boolean {
  return /^https:\/\/r-\d+\.test\//i.test(url)
}

// 视频原片不可读时，确认文件仍存在后尝试刷新一次当前灯箱上下文。
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

// 记录视频错误并启动一次受控的自动恢复。
function handleVideoError() {
  videoError.value = true
  void tryAutoRecoverByReload().catch((error) => {
    console.warn('Failed to recover lightbox video:', error)
  })
}

// video 有首帧数据后淡入，poster 在此之前继续作为稳定底图。
function handleVideoLoadedData() {
  videoReady.value = true
}
</script>

<template>
  <div class="relative h-full w-full">
    <div
      ref="viewportRef"
      class="lightbox-video-viewport h-full w-full"
      style="touch-action: pan-y"
    >
      <div class="flex h-full w-full items-center justify-center">
        <div
          v-if="currentAsset && !videoError"
          :key="currentAsset.id"
          class="relative overflow-hidden"
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
  </div>
</template>
