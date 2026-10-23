<script setup lang="ts">
import { computed, nextTick, onScopeDispose, ref, watch } from 'vue'
import { galleryApi } from '../../api'
import { useGalleryLightbox, useLightboxImageViewport } from '../../composables'
import { useLightboxImagePreload } from '../../composables/useLightboxImagePreload'
import { useGalleryStore } from '../../store'
import type { Asset } from '../../types'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()
const store = useGalleryStore()
const lightbox = useGalleryLightbox()
const imagePreload = useLightboxImagePreload()
const emit = defineEmits<{
  ready: [assetId: number]
  'pannable-change': [pannable: boolean]
}>()

const imageError = ref(false)
const originalLoaded = ref(false)
// 仅在同一张图原图 load 完成时启用淡入；切图和复原交接不做过渡，避免叠帧。
const originalOpacityTransition = ref(false)
const autoRecovering = ref(false)
// 原图已预热时直接切换，否则等目标缩略图加载后再替换当前画面。
const displayAssetId = ref<number | null>(null)
const originalUrl = ref('')
let displaySwapToken = 0

// DOM 引用由组件持有，再交给视口 composable 处理交互和测量。
const lightboxRootRef = ref<HTMLElement | null>(null)
const viewportRef = ref<HTMLElement | null>(null)
const stageRef = ref<HTMLElement | null>(null)
const thumbnailImageRef = ref<HTMLImageElement | null>(null)
const originalImageRef = ref<HTMLImageElement | null>(null)
const restoreThumbnailRef = ref<HTMLImageElement | null>(null)

// 从 Pinia 当前索引读取业务资产；displayAsset 负责跟随加载进度实际渲染。
const currentAsset = computed(() => {
  const currentIdx = store.selection.activeIndex
  if (currentIdx === undefined) {
    return null
  }

  return store.getAssetAt(currentIdx)
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

const fitMode = computed(() => store.lightbox.fitMode)
const actualZoom = computed(() => store.lightbox.zoom)
const rotationDegrees = computed(() => store.lightbox.rotationDegrees)

const {
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
  lightboxRootRef,
  viewportRef,
  stageRef,
  thumbnailImageRef,
  originalImageRef,
  restoreThumbnailRef,
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
async function commitDisplayAsset(asset: Asset) {
  const token = displaySwapToken
  originalOpacityTransition.value = false
  resetImageInteraction()
  displayAssetId.value = asset.id
  originalUrl.value = galleryApi.getAssetUrl(asset)
  // 已解码的原图从首帧起就可见，不再先隐藏原图、等待 load 后淡入。
  originalLoaded.value = imagePreload.getReadyImage(asset) !== undefined
  imageError.value = false

  await nextTick()
  if (token !== displaySwapToken) {
    return
  }
  // 等 viewport 重新布局后再校正滚动位置。
  syncViewportPosition()
  // Pager 用这个事件结束 pending navigation，而不是依赖固定延时。
  emit('ready', asset.id)
}

// 只消费已经就绪的原图，不等待正在进行的预加载，保持切图响应优先。
watch(
  [
    () => store.selection.activeIndex,
    () => currentAsset.value?.id,
    () => (currentAsset.value ? galleryApi.getAssetUrl(currentAsset.value) : ''),
  ],
  async ([activeIndex, targetId, targetUrl]) => {
    const token = ++displaySwapToken
    if (activeIndex === undefined) {
      // 灯箱关闭或没有焦点时清空当前渲染对象。
      resetImageInteraction()
      displayAssetId.value = null
      return
    }

    if (targetId === undefined) {
      return
    }

    const asset = currentAsset.value
    if (!asset) {
      return
    }

    if (displayAssetId.value === null) {
      // 首次挂载无需等待旧图，直接显示当前资源。
      await commitDisplayAsset(asset)
      return
    }

    if (displayAssetId.value === targetId && originalUrl.value === targetUrl) {
      // 快速切出去又返回时可能沿用同一个已显示节点，不会再有新的 load 事件。
      if (originalLoaded.value) {
        void notifyOriginalReady()
      }
      return
    }

    if (!imagePreload.getReadyImage(asset)) {
      // 未命中预加载时沿用缩略图交接，不把原图请求变成导航的等待条件。
      await preloadThumbnailForAsset(asset)
    }
    if (token !== displaySwapToken) {
      return
    }
    if (currentAsset.value?.id !== targetId) {
      // 等待期间如果用户又滑到了别的资源，丢弃这次过期切换。
      return
    }

    await commitDisplayAsset(asset)
  },
  { immediate: true, flush: 'post' }
)

// 与 Pager 的 ready 分开确认原图解码，过期的图片节点不能放行新目标的后台任务。
async function notifyOriginalReady() {
  const image = originalImageRef.value
  const assetId = displayAssetId.value
  const url = originalUrl.value
  const token = displaySwapToken
  if (!image || assetId === null) {
    return
  }

  try {
    await image.decode()
  } catch {
    // 解码未确认时不放行相邻预加载，原图显示和既有错误恢复仍由媒体自身处理。
    return
  }
  if (token !== displaySwapToken || image !== originalImageRef.value || imageError.value) {
    return
  }
  imagePreload.notifyCurrentMediaReady(assetId, url)
}

// 显示仍保持响应优先，不让原图解码确认阻塞缩略图或已缓存原图的交接。
function handleOriginalLoad(event: Event) {
  if (event.target !== originalImageRef.value) {
    return
  }
  if (!originalLoaded.value) {
    originalOpacityTransition.value = true
    originalLoaded.value = true
  }
  void notifyOriginalReady()
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
function handleImageError(event: Event) {
  if (event.target !== originalImageRef.value) {
    return
  }
  resetImageInteraction()
  imageError.value = true

  void tryAutoRecoverByReload().catch((error) => {
    console.warn('Failed to recover lightbox image:', error)
  })
}

onScopeDispose(() => {
  displaySwapToken += 1
})

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
            :key="`${displayAsset.id}:${originalUrl}`"
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
