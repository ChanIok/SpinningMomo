<script setup lang="ts">
import { ref, computed, watch, onBeforeUnmount } from 'vue'
import { Play } from 'lucide-vue-next'
import { hexToHsv, hsvToHex, normalizeToHex } from '@/components/ui/color-picker/colorUtils'
import { useGalleryData } from '../../composables/useGalleryData'
import { useGalleryStore } from '../../store'
import MediaStatusChips from './MediaStatusChips.vue'
import type { Asset } from '../../types'

const FALLBACK_PLACEHOLDER_COLOR = '#6B7280'
const MAX_CONCURRENT_ORIGINAL_CARD_LOADS = 2
let activeOriginalCardLoads = 0
const pendingOriginalCardLoadStarters: Array<() => void> = []

// 释放一个原图加载槽，并启动队列里仍然有效的下一项。
function releaseOriginalCardLoadSlot() {
  activeOriginalCardLoads = Math.max(0, activeOriginalCardLoads - 1)

  // 已取消的排队项会自检失效并退出，因此这里持续推进到槽位再次占满。
  while (
    activeOriginalCardLoads < MAX_CONCURRENT_ORIGINAL_CARD_LOADS &&
    pendingOriginalCardLoadStarters.length > 0
  ) {
    pendingOriginalCardLoadStarters.shift()?.()
  }
}

// 申请一个共享加载槽，避免一屏原图同时进入解码和纹理上传。
function acquireOriginalCardLoadSlot(
  isRequestCurrent: () => boolean
): Promise<(() => void) | null> {
  return new Promise((resolve) => {
    const start = () => {
      if (!isRequestCurrent()) {
        resolve(null)
        return
      }

      activeOriginalCardLoads += 1
      let released = false
      resolve(() => {
        if (released) {
          return
        }

        released = true
        releaseOriginalCardLoadSlot()
      })
    }

    // 有空槽时立即开始；否则等待正在进行的原图加载释放槽位。
    if (activeOriginalCardLoads < MAX_CONCURRENT_ORIGINAL_CARD_LOADS) {
      start()
      return
    }

    pendingOriginalCardLoadStarters.push(start)
  })
}

// Props 定义
interface AssetCardProps {
  asset: Asset
  isSelected?: boolean
  aspectRatio?: string
  allowOriginalLoad?: boolean
}

const props = withDefaults(defineProps<AssetCardProps>(), {
  isSelected: false,
  aspectRatio: '1 / 1',
  allowOriginalLoad: false,
})

// Emits 定义
const emit = defineEmits<{
  click: [asset: Asset, event: MouseEvent]
  'double-click': [asset: Asset, event: MouseEvent]
  'context-menu': [asset: Asset, event: MouseEvent]
  'drag-start': [asset: Asset, event: DragEvent]
}>()

// 响应式状态
const isImageLoading = ref(true)
const imageError = ref(false)
const hasThumbnailRendered = ref(false)
const failedOriginalUrl = ref('')
const isShowingOriginal = ref(false)
let imageRequestVersion = 0
let originalPreloadVersion = 0
let originalPreloadImage: HTMLImageElement | null = null
let originalPreloadQueued = false
let releaseOriginalPreloadSlot: (() => void) | null = null

const { getAssetThumbnailUrl, getAssetUrl } = useGalleryData()
const store = useGalleryStore()
const useOriginalImagesForCards = computed(
  () => store.gallerySettings.view.useOriginalImagesForCards
)
const showRatingBadge = computed(() => store.gallerySettings.view.showRatingBadge)
const showTagBadges = computed(() => store.gallerySettings.view.showTagBadges)
const showDyeCodeBadge = computed(
  () => store.gallerySettings.view.showDyeCodeBadge && store.dyeCodeAssetIds.has(props.asset.id)
)
const assetTags = computed(() =>
  showTagBadges.value ? (store.assetTagsById.get(props.asset.id) ?? []) : []
)

const thumbnailUrl = computed(() => getAssetThumbnailUrl(props.asset))
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
    failedOriginalUrl.value !== originalUrl.value
)
const hasThumbnail = computed(() => thumbnailUrl.value.length > 0)
const enableHoverScale = computed(() => !useOriginalImagesForCards.value)
const isVideoAsset = computed(() => props.asset.type === 'video')

const showPlaceholder = computed(
  () => imageError.value || !hasThumbnail.value || !hasThumbnailRendered.value
)

const placeholderColor = computed(() => {
  return getAdjustedPlaceholderColor(props.asset.dominantColorHex)
})

watch(
  [() => props.asset.id, thumbnailUrl],
  () => {
    cancelOriginalPreload()

    // 新素材或缩略图变化后回到初始路径，避免复用上一张卡片的显示状态。
    imageRequestVersion += 1
    hasThumbnailRendered.value = false
    failedOriginalUrl.value = ''
    isShowingOriginal.value = false
    imageError.value = false
    isImageLoading.value = thumbnailUrl.value.length > 0
  },
  { immediate: true }
)

watch(
  [supportsOriginalCardImage, originalUrl],
  () => {
    cancelOriginalPreload()

    // 原图设置或 URL 变化只重置原图层，不影响已经显示的缩略图。
    failedOriginalUrl.value = ''
    isShowingOriginal.value = false
  },
  { immediate: true }
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
      cancelOriginalPreload()
    }
  },
  { immediate: true }
)

onBeforeUnmount(() => {
  cancelOriginalPreload()
})

// 事件处理
function handleClick(event: MouseEvent) {
  emit('click', props.asset, event)
}

function handleDoubleClick(event: MouseEvent) {
  emit('double-click', props.asset, event)
}

function handleContextMenu(event: MouseEvent) {
  // 素材菜单需要等待选区准备完成；先同步截断冒泡，避免内容区背景菜单抢先打开。
  event.preventDefault()
  event.stopPropagation()
  emit('context-menu', props.asset, event)
}

function handleDragStart(event: DragEvent) {
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
}

// 取消本次挂载中尚未完成的原图预加载。
function cancelOriginalPreload() {
  originalPreloadVersion += 1
  originalPreloadQueued = false
  releaseOriginalPreloadSlot?.()
  releaseOriginalPreloadSlot = null

  if (!originalPreloadImage) {
    return
  }

  // 清掉回调和 src，让滚动开始后未完成的请求不再推动 UI 状态。
  originalPreloadImage.onload = null
  originalPreloadImage.onerror = null
  originalPreloadImage.src = ''
  originalPreloadImage = null
}

// 在离屏 Image 中预加载并解码原图，完成后才允许当前卡片切换显示。
async function startOriginalUpgrade() {
  if (!canStartOriginalUpgrade.value || originalPreloadQueued || originalPreloadImage) {
    return
  }

  // 用版本号隔离滚动取消、素材切换和异步排队完成之间的竞态。
  const requestVersion = ++originalPreloadVersion
  const requestUrl = originalUrl.value
  originalPreloadQueued = true

  const releaseSlot = await acquireOriginalCardLoadSlot(
    () =>
      requestVersion === originalPreloadVersion &&
      requestUrl === originalUrl.value &&
      canStartOriginalUpgrade.value
  )

  if (!releaseSlot) {
    if (requestVersion === originalPreloadVersion) {
      originalPreloadQueued = false
    }
    return
  }

  if (
    requestVersion !== originalPreloadVersion ||
    requestUrl !== originalUrl.value ||
    !canStartOriginalUpgrade.value
  ) {
    originalPreloadQueued = false
    releaseSlot()
    return
  }

  const preloadImage = new Image()
  originalPreloadQueued = false
  releaseOriginalPreloadSlot = releaseSlot
  originalPreloadImage = preloadImage

  preloadImage.decoding = 'async'
  preloadImage.onload = () => {
    void finishOriginalUpgrade(preloadImage, requestUrl, requestVersion)
  }
  preloadImage.onerror = () => {
    if (requestVersion !== originalPreloadVersion) {
      return
    }

    // 失败只记录当前原图 URL，不影响缩略图继续显示。
    releaseOriginalPreloadSlot?.()
    releaseOriginalPreloadSlot = null
    failedOriginalUrl.value = requestUrl
    originalPreloadImage = null
  }
  preloadImage.src = requestUrl
}

// 完成原图升级：等待解码结束，再把本次挂载的显示源切到原图。
async function finishOriginalUpgrade(
  preloadImage: HTMLImageElement,
  requestUrl: string,
  requestVersion: number
) {
  try {
    await preloadImage.decode()
  } catch {
    // load 已成功时，decode 拒绝不应破坏缩略图路径；继续让浏览器使用已接受的图像。
  }

  if (
    requestVersion !== originalPreloadVersion ||
    requestUrl !== originalUrl.value ||
    !props.allowOriginalLoad ||
    !supportsOriginalCardImage.value
  ) {
    releaseOriginalPreloadSlot?.()
    releaseOriginalPreloadSlot = null
    return
  }

  // 只有仍处于本轮滚动空闲许可内，才把可见卡片升级为原图。
  releaseOriginalPreloadSlot?.()
  releaseOriginalPreloadSlot = null
  originalPreloadImage = null
  imageError.value = false
  isShowingOriginal.value = true
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
    class="group transition-ring relative w-full overflow-hidden rounded-md bg-background duration-200 contain-[layout_size_paint] select-none"
    :class="[
      {
        'ring-4 ring-primary': isSelected,
        'shadow-md hover:shadow-lg': !isSelected,
        'shadow-lg': isSelected,
      },
    ]"
    :style="{ aspectRatio: props.aspectRatio }"
    @click="handleClick"
    @dblclick="handleDoubleClick"
    @contextmenu="handleContextMenu"
    @dragstart="handleDragStart"
  >
    <!-- 卡片图像容器 -->
    <div data-asset-thumbnail class="relative h-full w-full overflow-hidden rounded-md">
      <!-- 缩略图是卡片的基础显示层，主色占位只服务它的首次加载。 -->
      <img
        v-if="hasThumbnail && !imageError"
        :src="thumbnailUrl"
        :alt="asset.name"
        loading="eager"
        class="h-full w-full object-cover"
        :class="{
          'transition-transform duration-200 group-hover:scale-105': enableHoverScale,
        }"
        @load="onThumbnailLoad"
        @error="onThumbnailError"
      />

      <!-- 原图只作为已预加载完成后的覆盖层；加载失败时自然露出下面的缩略图。 -->
      <img
        v-if="isShowingOriginal && supportsOriginalCardImage"
        :src="originalUrl"
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
        class="absolute inset-x-0 bottom-0 flex items-end justify-start bg-gradient-to-t from-black/50 via-black/10 to-transparent p-3"
      >
        <div
          class="flex h-8 w-8 items-center justify-center rounded-full border border-white/20 bg-black/55 text-white shadow-sm backdrop-blur-sm"
        >
          <Play class="ml-0.5 h-4 w-4 fill-current" />
        </div>
      </div>

      <MediaStatusChips
        :rating="asset.rating"
        :review-flag="asset.reviewFlag"
        :show-rating="showRatingBadge"
        :has-dye-code="showDyeCodeBadge"
        :show-tags="showTagBadges"
        :tags="assetTags"
      />

      <!-- 选择指示器 -->
      <div
        v-if="isSelected"
        data-selection-indicator
        class="absolute top-2 right-2 flex h-6 w-6 items-center justify-center rounded-full bg-primary text-primary-foreground shadow-sm"
      >
        <svg class="h-4 w-4" fill="currentColor" viewBox="0 0 20 20">
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
