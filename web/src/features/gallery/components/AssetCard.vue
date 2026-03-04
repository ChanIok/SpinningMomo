<script setup lang="ts">
import { ref, computed } from 'vue'
import { useGalleryData } from '../composables/useGalleryData'
import type { Asset } from '../types'

// Props 定义
interface AssetCardProps {
  asset: Asset
  isSelected?: boolean
  showName?: boolean
  showSize?: boolean
}

const props = withDefaults(defineProps<AssetCardProps>(), {
  isSelected: false,
  showName: true,
  showSize: false,
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

function formatFileSize(bytes: number): string {
  const units = ['B', 'KB', 'MB', 'GB']
  let size = bytes
  let unitIndex = 0

  while (size >= 1024 && unitIndex < units.length - 1) {
    size /= 1024
    unitIndex++
  }

  return `${size.toFixed(unitIndex === 0 ? 0 : 1)}${units[unitIndex]}`
}
</script>

<template>
  <div
    class="group relative w-full overflow-hidden rounded bg-background transition-all duration-200 contain-[layout_size_paint] select-none"
    :class="[
      {
        'ring-2 ring-primary ring-offset-2': isSelected,
        'shadow-md hover:shadow-lg': !isSelected,
        'shadow-lg': isSelected,
      },
    ]"
    style="aspect-ratio: 1/1"
    @click="handleClick"
    @dblclick="handleDoubleClick"
    @contextmenu="handleContextMenu"
  >
    <!-- 缩略图容器 -->
    <div class="relative h-full w-full overflow-hidden">
      <!-- 缩略图 -->
      <img
        v-if="!imageError"
        :src="thumbnailUrl"
        :alt="asset.name"
        class="h-full w-full object-cover transition-transform duration-200 group-hover:scale-105"
        @load="onImageLoad"
        @error="onImageError"
      />

      <!-- 加载占位符 -->
      <div
        v-if="isImageLoading"
        class="absolute inset-0 flex animate-pulse items-center justify-center bg-muted"
      >
        <div class="text-2xl text-muted-foreground">📷</div>
      </div>

      <!-- 错误占位符 -->
      <div
        v-if="imageError"
        class="absolute inset-0 flex flex-col items-center justify-center bg-muted text-muted-foreground"
      >
        <div class="mb-1 text-2xl">❌</div>
        <div class="px-2 text-center text-xs">加载失败</div>
      </div>

      <!-- 遮罩层 -->
      <div
        class="absolute inset-0 bg-black/0 transition-all duration-200"
        :class="{
          'bg-black/20': isSelected,
          'group-hover:bg-black/10': !isSelected,
        }"
      />

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

      <!-- 资产类型标识 -->
      <div
        class="absolute right-0 bottom-0 left-0 bg-gradient-to-t from-black/60 to-transparent p-2"
      >
        <div class="flex items-center justify-between text-white">
          <!-- 文件大小（可选） -->
          <span v-if="showSize && asset.size" class="text-xs opacity-80">
            {{ formatFileSize(asset.size) }}
          </span>
        </div>

        <!-- 文件名（在较大尺寸时显示） -->
        <div v-if="showName" class="mt-1">
          <div class="truncate text-xs text-white/90" :title="asset.name">
            {{ asset.name }}
          </div>
        </div>
      </div>
    </div>
  </div>
</template>
