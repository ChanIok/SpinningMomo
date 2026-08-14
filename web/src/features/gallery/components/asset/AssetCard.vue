<script setup lang="ts">
import { ref, computed, watch, onBeforeUnmount } from 'vue'
import { Play } from '@lucide/vue'
import { hexToHsv, hsvToHex, normalizeToHex } from '@/components/ui/color-picker/colorUtils'
import { useGalleryData } from '../../composables/useGalleryData'
import { useOriginalPreviewWorker } from '../../composables/useOriginalPreviewWorker'
import { useGalleryStore } from '../../store'
import MediaStatusChips from './MediaStatusChips.vue'
import type { Asset } from '../../types'
import {
  isGalleryTouchContextMenu,
  isGalleryTouchInput,
  isGalleryScrollRecent,
  normalizeGalleryInputType,
  type GalleryInputType,
} from '../../input'

const FALLBACK_PLACEHOLDER_COLOR = '#6B7280'

// Props 定义
interface AssetCardProps {
  asset: Asset
  isSelected?: boolean
  aspectRatio?: string
  allowThumbnailLoad?: boolean
  allowOriginalLoad?: boolean
  originalPreviewShortEdge?: number
}

const props = withDefaults(defineProps<AssetCardProps>(), {
  isSelected: false,
  aspectRatio: '1 / 1',
  allowThumbnailLoad: true,
  allowOriginalLoad: false,
  originalPreviewShortEdge: 0,
})

// Emits 定义
const emit = defineEmits<{
  click: [asset: Asset, event: MouseEvent, inputType: GalleryInputType]
  'double-click': [asset: Asset, event: MouseEvent, inputType: GalleryInputType]
  'long-press': [asset: Asset, event: PointerEvent]
  'context-menu': [asset: Asset, event: MouseEvent]
  'drag-start': [asset: Asset, event: DragEvent]
}>()

// 响应式状态
const isImageLoading = ref(true)
const imageError = ref(false)
const hasThumbnailRendered = ref(false)
const failedOriginalUrl = ref('')
const isShowingOriginal = ref(false)
const originalPreviewUrl = ref('')
let imageRequestVersion = 0
let originalPreloadVersion = 0
let originalPreviewAbortController: AbortController | null = null
// 垂直滚动优先于长按；不捕获触摸指针，让浏览器在滚动开始时接管手势。
const LONG_PRESS_DELAY = 500
const LONG_PRESS_MOVE_THRESHOLD = 16
let longPressTimer: ReturnType<typeof setTimeout> | null = null
let longPressStartPoint: { x: number; y: number } | null = null
let longPressTriggered = false
let longPressBlocked = false
let lastInputType: GalleryInputType = 'mouse'

const { getAssetThumbnailUrl, getAssetUrl } = useGalleryData()
const { generateOriginalPreview } = useOriginalPreviewWorker()
const store = useGalleryStore()
const useOriginalImagesForCards = computed(() => store.view.useOriginalImagesForCards)
const showRatingBadge = computed(() => store.view.showRatingBadge)
const showTagBadges = computed(() => store.view.showTagBadges)
const showDyeCodeBadge = computed(
  () => store.view.showDyeCodeBadge && store.dyeCodeAssetIds.has(props.asset.id)
)
const assetTags = computed(() =>
  showTagBadges.value ? (store.assetTagsById.get(props.asset.id) ?? []) : []
)

const thumbnailUrl = computed(() => getAssetThumbnailUrl(props.asset))
const scheduledThumbnailUrl = computed(() => (props.allowThumbnailLoad ? thumbnailUrl.value : ''))
const originalUrl = computed(() => getAssetUrl(props.asset))
const supportsOriginalCardImage = computed(
  () =>
    useOriginalImagesForCards.value && props.asset.type === 'photo' && originalUrl.value.length > 0
)
const canStartOriginalUpgrade = computed(
  () =>
    supportsOriginalCardImage.value &&
    props.allowOriginalLoad &&
    hasThumbnailRendered.value &&
    !isShowingOriginal.value &&
    hasOriginalPreviewShortEdge.value &&
    failedOriginalUrl.value !== originalUrl.value
)
const hasThumbnail = computed(() => scheduledThumbnailUrl.value.length > 0)
const hasOriginalPreviewShortEdge = computed(() => props.originalPreviewShortEdge > 0)
const enableHoverScale = computed(() => !useOriginalImagesForCards.value && !store.isCompactWindow)
const isVideoAsset = computed(() => props.asset.type === 'video')

const showPlaceholder = computed(
  () => imageError.value || !hasThumbnail.value || !hasThumbnailRendered.value
)

const placeholderColor = computed(() => {
  return getAdjustedPlaceholderColor(props.asset.dominantColorHex)
})

watch(
  [() => props.asset.id, scheduledThumbnailUrl],
  () => {
    resetOriginalPreview()

    // 新素材、缩略图 URL 或调度许可变化后回到基础显示路径。
    imageRequestVersion += 1
    hasThumbnailRendered.value = false
    failedOriginalUrl.value = ''
    isShowingOriginal.value = false
    imageError.value = false
    isImageLoading.value = scheduledThumbnailUrl.value.length > 0 || thumbnailUrl.value.length > 0
  },
  { immediate: true }
)

watch(
  [supportsOriginalCardImage, originalUrl],
  () => {
    resetOriginalPreview()

    // 原图设置或 URL 变化只重置原图层，不影响已经显示的缩略图。
    failedOriginalUrl.value = ''
    isShowingOriginal.value = false
  },
  { immediate: true }
)

watch(
  () => props.originalPreviewShortEdge,
  () => {
    // 卡片短边变化后重新生成匹配当前显示尺寸的临时预览图。
    resetOriginalPreview()
  }
)

watch(
  canStartOriginalUpgrade,
  (canStart) => {
    if (canStart) {
      void startOriginalUpgrade()
      return
    }

    // 滚动开始或设置关闭时，停止本次挂载里尚未完成的原图升级。
    if (!props.allowOriginalLoad || !supportsOriginalCardImage.value) {
      cancelOriginalPreviewRequest()
    }
  },
  { immediate: true }
)

onBeforeUnmount(() => {
  resetOriginalPreview()
  cancelLongPress()
})

// 事件处理
function handleClick(event: MouseEvent) {
  // 长按结束后浏览器仍可能补发 click；该 click 只属于长按手势，不应再打开暗房。
  if (longPressTriggered) {
    longPressTriggered = false
    event.preventDefault()
    event.stopPropagation()
    return
  }

  emit('click', props.asset, event, event.detail === 0 ? 'keyboard' : lastInputType)
}

function handleDoubleClick(event: MouseEvent) {
  // 窄屏触摸的单击已经打开暗房；不要让同一手势随后补发的 dblclick 再次触发。
  // 宽屏触摸仍允许双击打开暗房，以便单击可以先更新详情面板。
  if (store.isCompactWindow && isGalleryTouchInput(lastInputType)) {
    return
  }

  emit('double-click', props.asset, event, lastInputType)
}

function handlePointerDown(event: PointerEvent) {
  if (event.isPrimary === false) {
    return
  }

  lastInputType = normalizeGalleryInputType(event.pointerType)
  if (!isGalleryTouchInput(lastInputType)) {
    longPressTriggered = false
    longPressBlocked = false
    cancelLongPress()
    return
  }

  if (lastInputType === 'pen' && event.button !== 0) {
    return
  }

  longPressTriggered = false
  cancelLongPress()
  longPressBlocked = isGalleryScrollRecent()
  if (longPressBlocked) {
    return
  }

  longPressStartPoint = { x: event.clientX, y: event.clientY }
  longPressTimer = setTimeout(() => {
    longPressTimer = null
    if (longPressBlocked || longPressStartPoint === null || isGalleryScrollRecent()) {
      longPressBlocked = true
      return
    }

    longPressTriggered = true
    navigator.vibrate?.(10)
    emit('long-press', props.asset, event)
  }, LONG_PRESS_DELAY)
}

function handlePointerMove(event: PointerEvent) {
  if (!longPressStartPoint || longPressTriggered || longPressBlocked) {
    return
  }

  const distance = Math.hypot(
    event.clientX - longPressStartPoint.x,
    event.clientY - longPressStartPoint.y
  )
  if (distance > LONG_PRESS_MOVE_THRESHOLD) {
    longPressBlocked = true
    cancelLongPress()
  }
}

function handlePointerUp() {
  cancelLongPress()
  longPressBlocked = false
}

function cancelLongPress() {
  if (longPressTimer !== null) {
    clearTimeout(longPressTimer)
    longPressTimer = null
  }
  longPressStartPoint = null
}

function handleContextMenu(event: MouseEvent) {
  // 素材菜单需要等待选区准备完成；先同步截断冒泡，避免内容区背景菜单抢先打开。
  event.preventDefault()
  event.stopPropagation()
  const isLongPressGesture = longPressStartPoint !== null || longPressTriggered
  if (!isGalleryTouchContextMenu(event, lastInputType) && !isLongPressGesture) {
    emit('context-menu', props.asset, event)
  }
}

function handleDragStart(event: DragEvent) {
  if (lastInputType !== 'mouse') {
    event.preventDefault()
    return
  }
  emit('drag-start', props.asset, event)
}

// 缩略图加载完成后移除主色占位，后续原图升级继续用缩略图兜底。
async function onThumbnailLoad(event: Event) {
  const image = event.currentTarget as HTMLImageElement
  const requestVersion = imageRequestVersion
  const loadedUrl = image.src

  try {
    await image.decode()
  } catch {
    // load 已成功时，decode 拒绝不应转成加载失败；继续显示浏览器已接受的图像。
  }

  if (requestVersion !== imageRequestVersion || image.src !== loadedUrl) {
    return
  }

  // 缩略图真正可见后，主色占位不再参与原图升级阶段。
  hasThumbnailRendered.value = true
  isImageLoading.value = false
  imageError.value = false
}

// 缩略图加载失败时进入整卡错误态。
function onThumbnailError() {
  imageRequestVersion += 1

  isImageLoading.value = false
  imageError.value = true
}

// 原图覆盖层展示失败时退回缩略图，不影响已显示的缩略图。
function onOriginalImageError() {
  if (!isShowingOriginal.value || originalUrl.value.length === 0) {
    return
  }

  // 记录失败 URL，避免本次挂载反复尝试同一张原图。
  failedOriginalUrl.value = originalUrl.value
  isShowingOriginal.value = false
  revokeOriginalPreviewUrl()
}

// 取消本次挂载中尚未完成的高清预览任务。
function cancelOriginalPreviewRequest() {
  originalPreloadVersion += 1

  // 通过 AbortController 取消队列或 Worker 中仍可中断的阶段。
  originalPreviewAbortController?.abort()
  originalPreviewAbortController = null
}

// 重置当前卡片的高清预览状态，并释放临时 Blob URL。
function resetOriginalPreview() {
  cancelOriginalPreviewRequest()
  revokeOriginalPreviewUrl()
  isShowingOriginal.value = false
}

// 释放当前临时预览 URL，避免虚拟滚动反复挂载后泄漏 Blob。
function revokeOriginalPreviewUrl() {
  if (!originalPreviewUrl.value) {
    return
  }

  URL.revokeObjectURL(originalPreviewUrl.value)
  originalPreviewUrl.value = ''
}

// 请求 Worker 生成短边高清预览，完成后才允许当前卡片切换显示。
async function startOriginalUpgrade() {
  if (!canStartOriginalUpgrade.value || originalPreviewAbortController) {
    return
  }

  // 用版本号隔离滚动取消、素材切换和 Worker 结果返回之间的竞态。
  const requestVersion = ++originalPreloadVersion
  const requestUrl = originalUrl.value
  const targetShortEdge = getOriginalPreviewTargetShortEdge()
  const abortController = new AbortController()
  originalPreviewAbortController = abortController

  try {
    const previewBlob = await generateOriginalPreview({
      url: requestUrl,
      targetShortEdge,
      sourceWidth: props.asset.width,
      sourceHeight: props.asset.height,
      signal: abortController.signal,
    })

    if (!isOriginalPreviewRequestCurrent(requestVersion, requestUrl)) {
      return
    }

    const previewUrl = URL.createObjectURL(previewBlob)
    try {
      await preloadPreviewUrl(previewUrl, abortController.signal)

      if (!isOriginalPreviewRequestCurrent(requestVersion, requestUrl)) {
        URL.revokeObjectURL(previewUrl)
        return
      }

      // 新预览图确认可绘制后，再替换旧 URL 并显示覆盖层。
      revokeOriginalPreviewUrl()
      originalPreviewUrl.value = previewUrl
      imageError.value = false
      isShowingOriginal.value = true
    } catch (error) {
      URL.revokeObjectURL(previewUrl)
      throw error
    }
  } catch (error) {
    if (!isAbortError(error) && requestVersion === originalPreloadVersion) {
      // 失败只记录当前原图 URL，不影响缩略图继续显示。
      failedOriginalUrl.value = requestUrl
    }
  } finally {
    if (originalPreviewAbortController === abortController) {
      originalPreviewAbortController = null
    }
  }
}

// 计算 Worker 输出预览图的目标短边物理像素。
function getOriginalPreviewTargetShortEdge() {
  const pixelRatio = window.devicePixelRatio || 1
  return Math.max(1, Math.ceil(props.originalPreviewShortEdge * pixelRatio))
}

// 判断异步结果是否仍属于当前卡片和当前原图许可。
function isOriginalPreviewRequestCurrent(requestVersion: number, requestUrl: string): boolean {
  return (
    requestVersion === originalPreloadVersion &&
    requestUrl === originalUrl.value &&
    props.allowOriginalLoad &&
    supportsOriginalCardImage.value
  )
}

// 预解码 Worker 输出的 Blob URL，避免覆盖层刚插入时露出空表面。
async function preloadPreviewUrl(previewUrl: string, signal: AbortSignal): Promise<void> {
  if (signal.aborted) {
    throw createAbortError()
  }

  const previewImage = new Image()
  previewImage.decoding = 'async'

  await new Promise<void>((resolve, reject) => {
    const cleanup = () => {
      signal.removeEventListener('abort', handleAbort)
      previewImage.onload = null
      previewImage.onerror = null
    }
    const handleAbort = () => {
      cleanup()
      previewImage.src = ''
      reject(createAbortError())
    }

    previewImage.onload = () => {
      cleanup()
      resolve()
    }
    previewImage.onerror = () => {
      cleanup()
      reject(new Error('Failed to load generated original preview'))
    }
    signal.addEventListener('abort', handleAbort, { once: true })
    previewImage.src = previewUrl
  })

  try {
    await previewImage.decode()
  } catch {
    // load 已成功时，decode 拒绝不应破坏缩略图路径；继续让浏览器使用已接受的预览图。
  }
}

function isAbortError(error: unknown): boolean {
  return error instanceof DOMException && error.name === 'AbortError'
}

function createAbortError(): DOMException {
  return new DOMException('Original preview request was canceled', 'AbortError')
}

function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value))
}

function mixHexColors(baseHex: string, overlayHex: string, ratio: number): string {
  const base = normalizeToHex(baseHex, FALLBACK_PLACEHOLDER_COLOR)
  const overlay = normalizeToHex(overlayHex, '#FFFFFF')
  const weight = clamp(ratio, 0, 1)

  const channels = [0, 2, 4].map((offset) => {
    const baseValue = parseInt(base.slice(offset + 1, offset + 3), 16)
    const overlayValue = parseInt(overlay.slice(offset + 1, offset + 3), 16)
    return Math.round(baseValue * (1 - weight) + overlayValue * weight)
  })

  return `#${channels.map((value) => value.toString(16).padStart(2, '0')).join('')}`.toUpperCase()
}

function getAdjustedPlaceholderColor(hex?: string): string {
  const normalized = normalizeToHex(hex ?? '', FALLBACK_PLACEHOLDER_COLOR)
  const hsv = hexToHsv(normalized)

  const adjustedHex = hsvToHex({
    h: hsv.h,
    s: clamp(hsv.s, 18, 52),
    v: clamp(hsv.v, 38, 74),
  })

  return mixHexColors(adjustedHex, '#FFFFFF', 0.14)
}
</script>

<template>
  <div
    data-asset-card
    draggable="true"
    class="group transition-ring relative w-full overflow-hidden bg-background duration-200 contain-[layout_size_paint] select-none"
    :class="[
      store.isCompactWindow ? 'rounded-none shadow-none' : 'rounded-md',
      isSelected
        ? store.isCompactWindow
          ? 'ring-2 ring-primary ring-inset'
          : 'shadow-lg ring-4 ring-primary'
        : !store.isCompactWindow && 'shadow-md hover:shadow-lg',
    ]"
    :style="{ aspectRatio: props.aspectRatio, touchAction: 'pan-y' }"
    @click="handleClick"
    @dblclick="handleDoubleClick"
    @pointerdown="handlePointerDown"
    @pointermove="handlePointerMove"
    @pointerup="handlePointerUp"
    @pointercancel="handlePointerUp"
    @pointerleave="handlePointerUp"
    @contextmenu="handleContextMenu"
    @dragstart="handleDragStart"
  >
    <!-- 卡片图像容器 -->
    <div
      data-asset-thumbnail
      class="relative h-full w-full overflow-hidden"
      :class="store.isCompactWindow ? 'rounded-none' : 'rounded-md'"
    >
      <!-- 缩略图是卡片的基础显示层，主色占位只服务它的首次加载。 -->
      <img
        v-if="hasThumbnail && !imageError"
        :src="scheduledThumbnailUrl"
        :alt="asset.name"
        loading="eager"
        decoding="async"
        class="h-full w-full object-cover"
        :class="{
          'transition-transform duration-200 group-hover:scale-105': enableHoverScale,
        }"
        @load="onThumbnailLoad"
        @error="onThumbnailError"
      />

      <!-- 原图只作为已预加载完成后的覆盖层；加载失败时自然露出下面的缩略图。 -->
      <img
        v-if="isShowingOriginal && supportsOriginalCardImage && originalPreviewUrl"
        :src="originalPreviewUrl"
        :alt="asset.name"
        loading="eager"
        decoding="async"
        class="absolute inset-0 h-full w-full object-cover"
        :class="{
          'transition-transform duration-200 group-hover:scale-105': enableHoverScale,
        }"
        @error="onOriginalImageError"
      />

      <!-- 主色占位符 -->
      <div
        v-if="showPlaceholder"
        class="absolute inset-0"
        :style="{ backgroundColor: placeholderColor }"
      >
        <div class="absolute inset-0 bg-white/24 dark:bg-black/32" />
        <div
          v-if="isImageLoading"
          class="absolute inset-0 animate-pulse bg-gradient-to-br from-white/18 via-transparent to-black/10 dark:from-white/10 dark:to-black/18"
        />
      </div>

      <!-- 错误占位符 -->
      <div
        v-if="imageError"
        class="absolute inset-0 flex flex-col items-center justify-center text-white/88"
      >
        <div class="rounded-full border border-white/25 bg-black/15 p-2 backdrop-blur-[1px]">
          <svg
            xmlns="http://www.w3.org/2000/svg"
            width="16"
            height="16"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            stroke-width="2"
            stroke-linecap="round"
            stroke-linejoin="round"
          >
            <path d="M18 6 6 18" />
            <path d="m6 6 12 12" />
          </svg>
        </div>
        <div class="mt-2 px-2 text-center text-xs font-medium">加载失败</div>
      </div>

      <!-- 遮罩层 -->
      <div
        data-selection-mask
        class="absolute inset-0 bg-black/0 transition-all duration-200"
        :class="{
          'bg-black/30': isSelected,
          'group-hover:bg-black/10': !isSelected,
        }"
      />

      <div
        v-if="isVideoAsset"
        class="absolute inset-x-0 bottom-0 flex items-end justify-start bg-gradient-to-t from-black/50 via-black/10 to-transparent"
        :class="store.isCompactWindow ? 'p-1.5' : 'p-3'"
      >
        <div
          class="flex items-center justify-center rounded-full border border-white/20 bg-black/55 text-white shadow-sm backdrop-blur-sm"
          :class="store.isCompactWindow ? 'h-6 w-6' : 'h-8 w-8'"
        >
          <Play
            class="ml-0.5 fill-current"
            :class="store.isCompactWindow ? 'h-3 w-3' : 'h-4 w-4'"
          />
        </div>
      </div>

      <MediaStatusChips
        :rating="asset.rating"
        :review-flag="asset.reviewFlag"
        :dense="store.isCompactWindow"
        :show-rating="showRatingBadge"
        :has-dye-code="showDyeCodeBadge"
        :show-tags="showTagBadges"
        :tags="assetTags"
      />

      <!-- 选择指示器 -->
      <div
        v-if="isSelected"
        data-selection-indicator
        class="absolute flex items-center justify-center rounded-full bg-primary text-primary-foreground shadow-sm"
        :class="store.isCompactWindow ? 'top-1 right-1 h-5 w-5' : 'top-2 right-2 h-6 w-6'"
      >
        <svg
          class="h-4 w-4"
          :class="store.isCompactWindow && 'h-3 w-3'"
          fill="currentColor"
          viewBox="0 0 20 20"
        >
          <path
            fill-rule="evenodd"
            d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z"
            clip-rule="evenodd"
          />
        </svg>
      </div>
    </div>
  </div>
</template>
