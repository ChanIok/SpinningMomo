<script setup lang="ts">
import { computed, nextTick, onScopeDispose, ref, shallowRef, watch } from 'vue'
import { galleryApi } from '../../api'
import { useGalleryLightbox, useLightboxImageViewport } from '../../composables'
import {
  useLightboxImageLoading,
  type LightboxImageRequest,
} from '../../composables/useLightboxImageLoading'
import { useGalleryStore } from '../../store'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()
const store = useGalleryStore()
const lightbox = useGalleryLightbox()
const imageLoading = useLightboxImageLoading()
const emit = defineEmits<{
  'preview-ready': [request: LightboxImageRequest, thumbnailReady: boolean]
  'pannable-change': [pannable: boolean]
}>()

const imageError = ref(false)
const originalLoaded = ref(false)
// 原图保持静止，清晰度升级只让顶层缩略图淡出。
const thumbnailCoverVisible = ref(true)
const thumbnailOpacityTransition = ref(false)
const autoRecovering = ref(false)
// 新预览准备期间保留旧画面；加载任务由会话服务持有，组件只负责实际节点的交接。
const displayRequest = shallowRef<LightboxImageRequest | null>(null)
const originalUrl = ref('')
// 新目标先在独立图层准备；旧画面在目标提交前保持可见。
const pendingDisplayRequest = shallowRef<LightboxImageRequest | null>(null)
const pendingVisualUrl = computed(() => {
  const request = pendingDisplayRequest.value
  if (!request) return ''
  return request.original ? request.url : request.thumbnailUrl
})
let disposed = false
let pendingCommitToken = 0

// DOM 引用由组件持有，再交给视口 composable 处理交互和测量。
const lightboxRootRef = ref<HTMLElement | null>(null)
const viewportRef = ref<HTMLElement | null>(null)
const stageRef = ref<HTMLElement | null>(null)
const thumbnailImageRef = ref<HTMLImageElement | null>(null)
const originalImageRef = ref<HTMLImageElement | null>(null)
const pendingImageRef = ref<HTMLImageElement | null>(null)
const restoreThumbnailRef = ref<HTMLImageElement | null>(null)

const displayAsset = computed(() => {
  const request = displayRequest.value
  if (!request) return null
  // 加载任务保留画面身份；资产元数据仍跟随 Store 的就地更新和分页刷新。
  for (const assets of store.paginatedAssets.values()) {
    const asset = assets.find((item) => item.id === request.asset.id)
    if (asset) return asset
  }
  return request.asset
})
const thumbnailUrl = computed(() => displayRequest.value?.thumbnailUrl ?? '')

const fitMode = computed(() => store.lightbox.fitMode)
const actualZoom = computed(() => store.lightbox.zoom)
const rotationDegrees = computed(() => store.lightbox.rotationDegrees)

const {
  restoreThumbnailActive,
  fitScale,
  isPannable,
  canvasStyle,
  stageStyle,
  thumbnailLayerStyle,
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
  thumbnailCoverVisible,
  thumbnailOpacityTransition,
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

function isCurrentDisplay(request: LightboxImageRequest) {
  return (
    !disposed &&
    !store.lightbox.isClosing &&
    imageLoading.currentRequest.value === request &&
    displayRequest.value === request
  )
}

function isCurrentRequest(request: LightboxImageRequest) {
  return !disposed && !store.lightbox.isClosing && imageLoading.currentRequest.value === request
}

// 留出绘制机会，不用固定延时推测图片已经显示。
function waitForPaint(): Promise<void> {
  return new Promise((resolve) => {
    requestAnimationFrame(() => requestAnimationFrame(() => resolve()))
  })
}

function applyDisplayState(request: LightboxImageRequest, originalVisible = false) {
  imageError.value = request.preview === 'error' || request.originalError
  autoRecovering.value = false
  thumbnailOpacityTransition.value = false
  const hasReadyOriginal = request.original !== null
  thumbnailCoverVisible.value = !hasReadyOriginal && request.preview === 'thumbnail'
  originalLoaded.value = originalVisible
  originalUrl.value = hasReadyOriginal ? request.url : ''
  displayRequest.value = request
}

async function commitPreview(request: LightboxImageRequest) {
  if (displayRequest.value && displayRequest.value !== request) {
    await preparePendingPreview(request)
    return
  }

  resetImageInteraction()
  applyDisplayState(request)

  await nextTick()
  if (!isCurrentDisplay(request)) return

  const hasReadyOriginal = request.original !== null
  if (hasReadyOriginal) {
    // 相邻页已经显示清晰原图；只有当前节点完成接管后，Pager 才能移除相邻页。
    await revealOriginal(request)
    if (!isCurrentDisplay(request)) return
    syncViewportPosition()
    await nextTick()
    if (!isCurrentDisplay(request)) return
    emit('preview-ready', request, request.thumbnail !== null && !imageError.value)
    return
  }

  const image = thumbnailImageRef.value
  if (image) {
    try {
      // 确认实际显示节点可绘制；离屏准备成功不等于 DOM 画面已经就绪。
      await image.decode()
      if (!isCurrentDisplay(request)) return
    } catch {
      if (!isCurrentDisplay(request)) return
      imageError.value = true
    }
  }
  syncViewportPosition()
  await nextTick()
  if (!isCurrentDisplay(request)) return
  emit('preview-ready', request, request.thumbnail !== null && !imageError.value)
}

// 切图时先准备一个独立的目标图层。原图命中缓存时这里直接准备原图，绝不退回缩略图。
async function preparePendingPreview(request: LightboxImageRequest) {
  const token = ++pendingCommitToken
  resetImageInteraction()
  pendingDisplayRequest.value = request

  await nextTick()
  if (token !== pendingCommitToken || !isCurrentRequest(request)) return

  const image = pendingImageRef.value
  if (!image) return
  try {
    await image.decode()
  } catch {
    if (token === pendingCommitToken && isCurrentRequest(request)) {
      pendingDisplayRequest.value = null
      imageError.value = true
    }
    return
  }
  if (token !== pendingCommitToken || !isCurrentRequest(request)) return

  const hasReadyOriginal = request.original !== null
  applyDisplayState(request, hasReadyOriginal)

  await nextTick()
  if (token !== pendingCommitToken || !isCurrentDisplay(request)) return
  syncViewportPosition()
  await waitForPaint()
  if (token !== pendingCommitToken || !isCurrentDisplay(request)) return

  pendingDisplayRequest.value = null
  emit('preview-ready', request, !imageError.value)
}

// 原图只在服务允许并完成加载后接入 DOM；底层画面落地后，顶层缩略图淡出。
async function revealOriginal(request: LightboxImageRequest) {
  const original = request.original
  originalUrl.value = request.url
  await nextTick()
  if (!isCurrentDisplay(request) || request.original !== original) return
  const image = originalImageRef.value
  if (!image) return
  try {
    await image.decode()
  } catch {
    if (isCurrentDisplay(request)) showImageError(request)
    return
  }
  if (!isCurrentDisplay(request) || request.original !== original) return
  imageError.value = false
  originalLoaded.value = true
  await nextTick()
  await waitForPaint()
  if (!isCurrentDisplay(request)) return

  thumbnailOpacityTransition.value = thumbnailCoverVisible.value
  thumbnailCoverVisible.value = false
  await nextTick()
  // 包括过渡被缩小复原打断的情况，按实际动画结束而不是另设计时器。
  await Promise.all(
    (thumbnailImageRef.value?.getAnimations() ?? []).map((animation) =>
      animation.finished.catch(() => {})
    )
  )
}

watch(
  [
    () => imageLoading.currentRequest.value,
    () => imageLoading.currentRequest.value?.preview,
    () => imageLoading.currentRequest.value?.original,
  ],
  ([request]) => {
    if (!request) {
      if (store.lightbox.isOpen && store.selection.activeIndex !== undefined) return
      resetImageInteraction()
      pendingCommitToken += 1
      pendingDisplayRequest.value = null
      displayRequest.value = null
      originalUrl.value = ''
      return
    }
    if (request !== displayRequest.value && pendingDisplayRequest.value !== request) {
      pendingCommitToken += 1
      pendingDisplayRequest.value = null
    }
    if (request.preview === 'loading') return
    if (request !== displayRequest.value) {
      void commitPreview(request)
    } else if (request.original && !originalLoaded.value) {
      void revealOriginal(request)
    }
  },
  { immediate: true, flush: 'post' }
)

// 只对根映射 URL 做一次可达性确认，避免普通资源错误触发无限刷新。
async function tryAutoRecoverByReload(request: LightboxImageRequest) {
  if (autoRecovering.value || !/^https:\/\/r-\d+\.test\//i.test(request.url)) return
  const currentUrl = new URL(window.location.href)
  if (currentUrl.searchParams.get('lbRetry') === '1') return

  const reachability = await galleryApi.checkAssetReachable(request.asset.id)
  if (!isCurrentDisplay(request) || !reachability.exists || !reachability.readable) return

  autoRecovering.value = true
  currentUrl.searchParams.set('lbAssetId', String(request.asset.id))
  currentUrl.searchParams.set('lbFolderId', store.filter.folderId ?? 'all')
  currentUrl.searchParams.set('lbRetry', '1')
  window.location.replace(currentUrl.toString())
}

// 原图失败时显示错误画面，并尝试一次性恢复根映射。
function showImageError(request: LightboxImageRequest) {
  resetImageInteraction()
  imageError.value = true
  void tryAutoRecoverByReload(request).catch((error: unknown) => {
    console.warn('Failed to recover lightbox image:', error)
  })
}

watch(
  () => displayRequest.value?.originalError,
  (failed) => {
    const request = displayRequest.value
    if (failed && request && isCurrentDisplay(request)) showImageError(request)
  },
  { immediate: true }
)

onScopeDispose(() => {
  disposed = true
  pendingCommitToken += 1
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
          v-if="displayAsset"
          ref="stageRef"
          class="relative col-start-1 row-start-1 self-center justify-self-center select-none"
          :style="[stageStyle, imageError ? { visibility: 'hidden' } : undefined]"
          :title="zoomIndicator"
          @click="handleStageClick"
          @pointerdown="handleStagePointerDown"
          @pointermove="handleStagePointerMove"
          @pointerup="handleStagePointerUp"
          @pointercancel="handleStagePointerCancel"
          @lostpointercapture="handleStageLostPointerCapture"
        >
          <!-- 盒子由元数据宽高决定；底层原图不做淡入，顶层缩略图在解码交接后淡出。 -->
          <img
            v-if="originalUrl"
            :key="`${displayAsset.id}:${originalUrl}`"
            ref="originalImageRef"
            :src="originalUrl"
            :alt="displayAsset.name"
            :style="originalLayerStyle"
            class="absolute top-1/2 left-1/2 max-w-none object-fill select-none"
            draggable="false"
            @dragstart.prevent
          />

          <img
            v-if="displayRequest?.preview !== 'error'"
            :key="displayAsset.id"
            ref="thumbnailImageRef"
            :src="thumbnailUrl"
            :alt="displayAsset.name"
            :style="thumbnailLayerStyle"
            class="absolute top-1/2 left-1/2 max-w-none object-fill select-none"
            draggable="false"
            @dragstart.prevent
          />
        </div>

        <div
          v-if="imageError"
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

    <!-- 切图交接期间覆盖在旧画面之上；目标 stage 稳定后再移除，避免出现空白帧。 -->
    <img
      v-if="pendingDisplayRequest && pendingVisualUrl"
      ref="pendingImageRef"
      :src="pendingVisualUrl"
      :alt="pendingDisplayRequest.asset.name"
      class="pointer-events-none absolute inset-0 z-20 h-full w-full object-contain select-none"
      draggable="false"
      @dragstart.prevent
    />

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
