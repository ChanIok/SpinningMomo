<script setup lang="ts">
import { ref, computed, watch } from 'vue'
import { Play } from 'lucide-vue-next'
import { hexToHsv, hsvToHex, normalizeToHex } from '@/components/ui/color-picker/colorUtils'
import { useGalleryData } from '../composables/useGalleryData'
import MediaStatusChips from './MediaStatusChips.vue'
import type { Asset } from '../types'

const FALLBACK_PLACEHOLDER_COLOR = '#6B7280'

// Props 定义
interface AssetCardProps {
  asset: Asset
  isSelected?: boolean
  aspectRatio?: string
}

const props = withDefaults(defineProps<AssetCardProps>(), {
  isSelected: false,
  aspectRatio: '1 / 1',
})

// Emits 定义
const emit = defineEmits<{
  click: [asset: Asset, event: MouseEvent]
  'double-click': [asset: Asset, event: MouseEvent]
  'context-menu': [asset: Asset, event: MouseEvent]
}>()

// 响应式状态
const isImageLoading = ref(true)
const imageError = ref(false)

// 使用useGalleryData获取缩略图URL
const { getAssetThumbnailUrl } = useGalleryData()

// 缩略图URL - 从useGalleryData中获取
const thumbnailUrl = computed(() => {
  return getAssetThumbnailUrl(props.asset)
})

const hasThumbnail = computed(() => thumbnailUrl.value.length > 0)
const isVideoAsset = computed(() => props.asset.type === 'video')

const showPlaceholder = computed(
  () => isImageLoading.value || imageError.value || !hasThumbnail.value
)

const placeholderColor = computed(() => {
  return getAdjustedPlaceholderColor(props.asset.dominantColorHex)
})

watch(
  thumbnailUrl,
  (url) => {
    imageError.value = false
    isImageLoading.value = url.length > 0
  },
  { immediate: true }
)

// 事件处理
function handleClick(event: MouseEvent) {
  emit('click', props.asset, event)
}

function handleDoubleClick(event: MouseEvent) {
  emit('double-click', props.asset, event)
}

function handleContextMenu(event: MouseEvent) {
  emit('context-menu', props.asset, event)
}

// 图片加载处理
function onImageLoad() {
  isImageLoading.value = false
  imageError.value = false
}

function onImageError() {
  isImageLoading.value = false
  imageError.value = true
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
    class="group relative w-full overflow-hidden rounded bg-background transition-all duration-200 contain-[layout_size_paint] select-none"
    :class="[
      {
        'ring-2 ring-primary ring-offset-2': isSelected,
        'shadow-md hover:shadow-lg': !isSelected,
        'shadow-lg': isSelected,
      },
    ]"
    :style="{ aspectRatio: props.aspectRatio }"
    @click="handleClick"
    @dblclick="handleDoubleClick"
    @contextmenu="handleContextMenu"
  >
    <!-- 缩略图容器 -->
    <div class="relative h-full w-full overflow-hidden">
      <!-- 缩略图 -->
      <img
        v-if="hasThumbnail && !imageError"
        :src="thumbnailUrl"
        :alt="asset.name"
        class="h-full w-full object-cover transition-transform duration-200 group-hover:scale-105"
        @load="onImageLoad"
        @error="onImageError"
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
        class="absolute inset-0 bg-black/0 transition-all duration-200"
        :class="{
          'bg-black/20': isSelected,
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

      <MediaStatusChips :rating="asset.rating" :review-flag="asset.reviewFlag" />

      <!-- 选择指示器 -->
      <div
        v-if="isSelected"
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
