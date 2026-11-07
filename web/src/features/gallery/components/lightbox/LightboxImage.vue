<script setup lang="ts">
import { computed, nextTick, onScopeDispose, ref, shallowRef, watch } from 'vue'
import { galleryApi } from '../../api'
import { useGalleryData, useGalleryLightbox, useLightboxImageViewport } from '../../composables'
import {
  shouldUseLightboxTiling,
  waitForLightboxImageReady,
  waitForLightboxPaint,
  useLightboxImageLoading,
  type LightboxImageRequest,
} from '../../composables/useLightboxImageLoading'
import {
  useLightboxTiledRenderer,
  type LightboxTileRequest,
} from '../../composables/useLightboxTiledRenderer'
import { useGalleryStore } from '../../store'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()
const store = useGalleryStore()
const lightbox = useGalleryLightbox()
const galleryData = useGalleryData()
const imageLoading = useLightboxImageLoading()
const emit = defineEmits<{
  'preview-ready': [request: LightboxImageRequest, thumbnailReady: boolean]
  'pannable-change': [pannable: boolean]
}>()

const imageError = ref(false)
const originalLoaded = ref(false)
// 原图保持静止；普通图片接入 <img>，超大图片接入分块 canvas，清晰度升级只让顶层缩略图淡出。
const thumbnailCoverVisible = ref(true)
const thumbnailOpacityTransition = ref(false)
const autoRecovering = ref(false)
// 新预览准备期间保留旧画面；加载任务由会话服务持有，组件只负责实际节点的交接。
const displayRequest = shallowRef<LightboxImageRequest | null>(null)
type ImageRenderMode = 'image' | 'tiled'
const displayRenderMode = ref<ImageRenderMode>('image')
const originalUrl = ref('')
// 新目标先在独立图层准备；解码完成后同步显示新预览、隐藏旧画面。
const pendingDisplayRequest = shallowRef<LightboxImageRequest | null>(null)
const pendingPreviewReady = ref(false)
const pendingPreviewVisible = computed(
  () => pendingDisplayRequest.value !== null && pendingPreviewReady.value
)
const pendingVisualUrl = computed(() => {
  const request = pendingDisplayRequest.value
  if (!request) return ''
  if (shouldUseLightboxTiling(request.asset.width ?? 0, request.asset.height ?? 0)) {
    return request.thumbnailUrl
  }
  return request.original ? request.url : request.thumbnailUrl
})
let disposed = false
let pendingCommitToken = 0

// DOM 引用由组件持有，再交给视口 composable 处理交互和测量。
const lightboxRootRef = ref<HTMLElement | null>(null)
const viewportRef = ref<HTMLElement | null>(null)
const stageRef = ref<HTMLElement | null>(null)
const tiledCanvasRef = ref<HTMLCanvasElement | null>(null)
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
const displayScale = ref(1)
let handleTiledRuntimeFailure = () => {}

const tiledRenderer = useLightboxTiledRenderer({
  canvasRef: tiledCanvasRef,
  viewportRef,
  stageRef,
  displayScale,
  rotationDegrees,
  onRuntimeFailure: () => handleTiledRuntimeFailure(),
})
const useTiledRenderer = computed(() => displayRenderMode.value === 'tiled')
const tiledCanvasVisible = computed(
  () => useTiledRenderer.value && originalLoaded.value && !imageError.value
)

const {
  restoreThumbnailActive,
  restoreContentHidden,
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
  restoreThumbnailRef,
  waitForContentReady: () =>
    useTiledRenderer.value
      ? Promise.resolve(originalLoaded.value)
      : waitForLightboxImageReady(originalImageRef.value),
  waitForContentPainted: () =>
    useTiledRenderer.value
      ? tiledRenderer.paintCurrentFrame()
      : new Promise((resolve) => requestAnimationFrame(() => resolve(true))),
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

watch(
  [fitScale, fitMode, actualZoom],
  () => {
    displayScale.value = fitMode.value === 'contain' ? fitScale.value : actualZoom.value
  },
  { immediate: true }
)

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

// 判断请求是否仍同时拥有加载会话和当前显示层。
function isCurrentDisplay(request: LightboxImageRequest) {
  return (
    !disposed &&
    !store.lightbox.isClosing &&
    imageLoading.currentRequest.value === request &&
    displayRequest.value === request
  )
}

// 判断请求是否仍是当前暗房目标，即使它还没有提交到显示层。
function isCurrentRequest(request: LightboxImageRequest) {
  return !disposed && !store.lightbox.isClosing && imageLoading.currentRequest.value === request
}

// 根据 Asset 像素面积选择普通 img 或 GPU 分块路径。
function isTiledRequest(request: LightboxImageRequest) {
  return shouldUseLightboxTiling(request.asset.width ?? 0, request.asset.height ?? 0)
}

// 计算当前图前后相邻的预加载索引。
function getTiledNeighborIndexes() {
  const index = store.selection.activeIndex
  if (index === undefined || !store.view.preloadOriginalImagesForLightbox) return []
  return [index - 1, index + 1].filter((neighbor) => neighbor >= 0 && neighbor < store.totalCount)
}

// 读取相邻大图的 URL 和尺寸，供 renderer 复用同一套上传逻辑。
function getTiledNeighborTargets(): LightboxTileRequest[] {
  const indexes = getTiledNeighborIndexes()
  return indexes.flatMap((neighbor) => {
    const asset = store.getAssetAt(neighbor)
    if (
      !asset ||
      asset.type === 'video' ||
      !shouldUseLightboxTiling(asset.width ?? 0, asset.height ?? 0)
    ) {
      return []
    }
    return [
      {
        url: galleryApi.getAssetUrl(asset),
        width: asset.width ?? 0,
        height: asset.height ?? 0,
      },
    ]
  })
}

// 更新 tiled renderer 的保留范围，当前图和预加载邻图之外立即释放。
function retainTiledCacheRange(request: LightboxImageRequest) {
  const current = isTiledRequest(request) ? [request.url] : []
  tiledRenderer.retain([...current, ...getTiledNeighborTargets().map((target) => target.url)])
}

// 当前原图接管后，加载仍在范围内的相邻大图。
async function prefetchTiledNeighbors(request: LightboxImageRequest) {
  if (!store.view.preloadOriginalImagesForLightbox || !isCurrentDisplay(request)) return

  const indexes = getTiledNeighborIndexes()
  await galleryData.ensureIndexesLoaded(indexes)
  if (!isCurrentDisplay(request)) return

  // 分页数据补齐后重新计算保留范围和预加载目标。
  retainTiledCacheRange(request)
  const targets = getTiledNeighborTargets()
  for (const target of targets) {
    tiledRenderer.prefetch(target)
  }
}

// 提交当前请求的图片层状态，统一处理缩略图托底和普通原图挂载。
function applyDisplayState(request: LightboxImageRequest, originalVisible = false) {
  imageError.value = request.preview === 'error'
  autoRecovering.value = false
  thumbnailOpacityTransition.value = false
  const hasReadyOriginal = request.original !== null
  const tiled = isTiledRequest(request)
  thumbnailCoverVisible.value = tiled || !originalVisible
  originalLoaded.value = tiled ? false : originalVisible
  originalUrl.value = hasReadyOriginal && !tiled ? request.url : ''
  displayRenderMode.value = 'image'
  displayRequest.value = request
}

// 等待当前缩略图可见后通知 Pager，并开始当前原图接管流程。
async function commitPreview(request: LightboxImageRequest) {
  if (displayRequest.value && displayRequest.value !== request) {
    // 切图时先用独立 pending 层准备目标，旧画面暂时不动。
    await preparePendingPreview(request)
    return
  }

  // 当前请求首次提交时重置缩放和滚动状态。
  resetImageInteraction()
  applyDisplayState(request)

  await nextTick()
  if (!isCurrentDisplay(request)) return

  // 让真实 thumbnail 节点完成一次解码，离屏准备不等于可见节点已就绪。
  const image = thumbnailImageRef.value
  if (image) {
    if (!(await waitForLightboxImageReady(image)) && isCurrentDisplay(request)) {
      imageError.value = true
    }
    if (!isCurrentDisplay(request)) return
  }
  syncViewportPosition()
  await nextTick()
  if (!isCurrentDisplay(request)) return
  // 通知外层 Pager 可以结束入场交接，再异步加载原图。
  emit('preview-ready', request, request.thumbnail !== null && !imageError.value)
  void revealOriginal(request)
}

// 切图时准备独立目标层，超大原图继续由缩略图托底，避免目标加载期间出现空白帧。
async function preparePendingPreview(request: LightboxImageRequest) {
  const token = ++pendingCommitToken
  resetImageInteraction()
  pendingPreviewReady.value = false
  pendingDisplayRequest.value = request

  await nextTick()
  if (token !== pendingCommitToken || !isCurrentRequest(request)) return

  const image = pendingImageRef.value
  if (!image) return
  try {
    // pending 图层先完成解码，完成前继续显示上一张。
    await image.decode()
  } catch {
    if (token === pendingCommitToken && isCurrentRequest(request)) {
      pendingDisplayRequest.value = null
      imageError.value = true
    }
    return
  }
  if (token !== pendingCommitToken || !isCurrentRequest(request)) return

  pendingPreviewReady.value = true
  const originalVisible = request.original !== null
  // 目标预览独占画面后，再把 stage 布局提交给新请求。
  applyDisplayState(request, originalVisible)

  await nextTick()
  if (token !== pendingCommitToken || !isCurrentDisplay(request)) return
  if (originalVisible && !(await waitForLightboxImageReady(originalImageRef.value))) {
    if (token === pendingCommitToken && isCurrentDisplay(request)) {
      originalLoaded.value = false
      thumbnailCoverVisible.value = true
    }
  }
  if (token !== pendingCommitToken || !isCurrentDisplay(request)) return
  syncViewportPosition()
  await waitForLightboxPaint()
  if (token !== pendingCommitToken || !isCurrentDisplay(request)) return

  // 等新 stage 和缩略图落地后移除 pending 覆盖层。
  pendingDisplayRequest.value = null
  if (originalLoaded.value) {
    imageLoading.notifyOriginalPresented(request.asset.id, request)
  }
  emit('preview-ready', request, !imageError.value)
  void revealOriginal(request)
}

// 原图只在服务允许并完成加载后接入画面；底层画面落地后，顶层缩略图淡出。
let revealingRequest: LightboxImageRequest | null = null

// 等待外层预览呈现后，按 tiled 或普通 img 路径接管原图。
async function revealOriginal(request: LightboxImageRequest) {
  if (revealingRequest === request) return
  revealingRequest = request
  try {
    // 原图加载必须晚于 preview-presented，避免切图时抢占缩略图资源。
    const presented = await imageLoading.waitForPreviewPresented(request)
    if (!presented || !isCurrentDisplay(request)) return
    if (originalLoaded.value) return
    await revealOriginalInternal(request)
  } finally {
    if (revealingRequest === request) revealingRequest = null
  }
}

// 完成当前请求的 tiled 上传或普通原图解码，并淡出缩略图。
async function revealOriginalInternal(request: LightboxImageRequest) {
  if (isTiledRequest(request)) {
    const tileRequest: LightboxTileRequest = {
      url: request.url,
      width: request.asset.width ?? 0,
      height: request.asset.height ?? 0,
    }
    // tiled renderer 会在内部完成逐块上传和第一帧绘制。
    const result = await tiledRenderer.show(tileRequest)
    if (!isCurrentDisplay(request)) return
    if (result.kind === 'stale') return

    if (result.kind === 'ready') {
      // canvas 已经可以接管画面，再切换显示模式和缩略图透明度。
      imageError.value = false
      originalUrl.value = ''
      originalLoaded.value = true
      displayRenderMode.value = 'tiled'
      await nextTick()
      await waitForLightboxPaint()
      if (!isCurrentDisplay(request)) return

      thumbnailOpacityTransition.value = thumbnailCoverVisible.value
      thumbnailCoverVisible.value = false
      await nextTick()
      await Promise.all(
        (thumbnailImageRef.value?.getAnimations() ?? []).map((animation) =>
          animation.finished.catch(() => {})
        )
      )
      if (!isCurrentDisplay(request)) return
      imageLoading.notifyOriginalPresented(request.asset.id, request)
      void prefetchTiledNeighbors(request)
      return
    }

    // Worker、解码或 GPU 上传失败时恢复普通图片模式，继续走统一的 ensureOriginal 兜底。
    displayRenderMode.value = 'image'
    originalLoaded.value = false
    originalUrl.value = ''
    thumbnailOpacityTransition.value = false
    thumbnailCoverVisible.value = true
  }

  // 普通图片复用缓存或启动一次新的原图加载。
  const original = request.original ?? (await imageLoading.ensureOriginal(request))
  if (!isCurrentDisplay(request)) return
  if (!original) {
    handleOriginalFailure(request)
    return
  }

  // 挂载真实 img 后再次确认 DOM 节点对应的 URL 已经可绘制。
  originalUrl.value = request.url
  await nextTick()
  if (!isCurrentDisplay(request) || request.original !== original) return
  const image = originalImageRef.value
  if (!image) return
  if (!(await waitForLightboxImageReady(image))) {
    if (isCurrentDisplay(request)) handleOriginalFailure(request)
    return
  }
  if (!isCurrentDisplay(request) || request.original !== original) return
  imageError.value = false
  originalLoaded.value = true
  await nextTick()
  await waitForLightboxPaint()
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
  if (!isCurrentDisplay(request)) return
  // 真实原图完成接管后才通知加载服务预热邻图。
  imageLoading.notifyOriginalPresented(request.asset.id, request)
  void prefetchTiledNeighbors(request)
}

// tiled context 运行时丢失时切换当前请求到普通 img。
handleTiledRuntimeFailure = () => {
  const request = displayRequest.value
  if (!request || !isCurrentDisplay(request)) return
  displayRenderMode.value = 'image'
  originalLoaded.value = false
  originalUrl.value = ''
  thumbnailOpacityTransition.value = false
  thumbnailCoverVisible.value = true
  void revealOriginal(request)
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
      // 暗房真正关闭后释放全部 tiled GPU 资源和待处理交接。
      tiledRenderer.close()
      resetImageInteraction()
      pendingCommitToken += 1
      pendingDisplayRequest.value = null
      displayRequest.value = null
      originalUrl.value = ''
      return
    }
    if (request !== displayRequest.value && pendingDisplayRequest.value !== request) {
      // 新请求尚未呈现时只更新缓存保留范围，旧画面继续托底。
      retainTiledCacheRange(request)
      pendingCommitToken += 1
      pendingDisplayRequest.value = null
    }
    if (request.preview === 'loading') return
    if (request !== displayRequest.value) {
      // 缩略图就绪后提交当前预览层。
      void commitPreview(request)
    } else if (request.original && !originalLoaded.value) {
      // 离屏原图后来完成时，尝试让它接管当前显示层。
      void revealOriginal(request)
    }
  },
  { immediate: true, flush: 'post' }
)

watch(
  [() => store.view.preloadOriginalImagesForLightbox, () => store.paginatedAssetsVersion],
  () => {
    // 预加载开关或分页数据变化后重新计算 tiled cache 保留范围。
    const request = imageLoading.currentRequest.value
    if (request && isCurrentRequest(request)) retainTiledCacheRange(request)
  }
)

// 只对根映射 URL 做一次可达性确认，避免普通资源错误触发无限刷新。
// 检查根映射 URL 是否可达，必要时刷新一次当前资源映射。
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

// 原图失败时统一尝试一次根映射恢复。
function retryRootMapping(request: LightboxImageRequest) {
  void tryAutoRecoverByReload(request).catch((error: unknown) => {
    console.warn('Failed to recover lightbox image:', error)
  })
}

// 没有可见缩略图时显示错误，否则保留缩略图并尝试恢复资源映射。
function showImageError(request: LightboxImageRequest) {
  resetImageInteraction()
  imageError.value = true
  retryRootMapping(request)
}

// 将原图失败转换成缩略图托底或错误状态。
function handleOriginalFailure(request: LightboxImageRequest) {
  originalLoaded.value = false
  thumbnailOpacityTransition.value = false
  thumbnailCoverVisible.value = true
  if (!request.thumbnail) showImageError(request)
  else retryRootMapping(request)
}

watch(
  () => displayRequest.value?.originalError,
  (failed) => {
    const request = displayRequest.value
    if (failed && request && isCurrentDisplay(request)) handleOriginalFailure(request)
  },
  { immediate: true }
)

onScopeDispose(() => {
  disposed = true
  pendingCommitToken += 1
  tiledRenderer.close()
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
      :style="{ visibility: pendingPreviewVisible ? 'hidden' : undefined }"
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
          <!-- 盒子由元数据宽高决定；普通原图不做淡入，超大原图由外层 canvas 接管。 -->
          <img
            v-if="originalUrl && !useTiledRenderer"
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

    <canvas
      ref="tiledCanvasRef"
      class="pointer-events-none absolute inset-0 z-[15] h-full w-full"
      :style="{
        visibility:
          tiledCanvasVisible && !restoreContentHidden && !pendingPreviewVisible
            ? 'visible'
            : 'hidden',
      }"
    />

    <!-- 解码完成后独占画面；目标 stage 稳定后再移除，隐藏期间保留节点供 decode 使用。 -->
    <img
      v-if="pendingDisplayRequest && pendingVisualUrl"
      ref="pendingImageRef"
      :src="pendingVisualUrl"
      :alt="pendingDisplayRequest.asset.name"
      class="pointer-events-none absolute inset-0 z-20 h-full w-full object-contain select-none"
      :style="{ visibility: pendingPreviewVisible ? 'visible' : 'hidden' }"
      draggable="false"
      @dragstart.prevent
    />

    <!-- 双击缩放统一使用缩略图代理；保持缩略图自身尺寸，避免把 8K 图片盒子作为动画图层。 -->
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
