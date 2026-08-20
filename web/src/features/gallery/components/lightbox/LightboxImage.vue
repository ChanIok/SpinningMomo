<script setup lang="ts">
import { computed, nextTick, ref, watch } from 'vue'
import { galleryApi } from '../../api'
import { useGalleryData, useGalleryLightbox, useLightboxImageViewport } from '../../composables'
import { useGalleryStore } from '../../store'
import type { Asset } from '../../types'
import { useI18n } from '@/composables/useI18n'

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
const autoRecovering = ref(false)
// 实际渲染的资产 id；切图时滞后于 selection，直到目标缩略图 decode 完成再切换。
const displayAssetId = ref<number | null>(null)
let displaySwapToken = 0

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

const {
  lightboxRootRef,
  viewportRef,
  stageRef,
  thumbnailImageRef,
  originalImageRef,
  restoreThumbnailRef,
  restoreThumbnailActive,
  fitScale,
  isPannable,
  canvasStyle,
  stageStyle,
  imageLayerStyle,
  originalLayerStyle,
  resetImageInteraction,
  syncViewportPosition,
  handleStageClick,
  handleStagePointerDown,
  handleStagePointerMove,
  handleStagePointerUp,
  handleStagePointerCancel,
  handleStageLostPointerCapture,
  handleViewportWheel,
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
} = useLightboxImageViewport({
  displayAsset,
  imageError,
  fitMode,
  actualZoom,
  rotationDegrees,
  originalLoaded,
  originalOpacityTransition,
  lightbox,
})

const zoomIndicator = computed(() => {
  if (fitMode.value === 'contain') {
    return t('gallery.lightbox.image.fitIndicator', { percent: Math.round(fitScale.value * 100) })
  }

  return `${Math.round(actualZoom.value * 100)}%`
})

// 把“图片是否会抢走拖拽 pointer”同步给外层 Pager。
watch(
  isPannable,
  (pannable) => {
    emit('pannable-change', pannable)
  },
  { immediate: true }
)

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
  resetImageInteraction()
  displayAssetId.value = assetId
  originalLoaded.value = false
  imageError.value = false

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
      resetImageInteraction()
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
  resetImageInteraction()
  imageError.value = true

  void tryAutoRecoverByReload().catch((error) => {
    console.warn('Failed to recover lightbox image:', error)
  })
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
