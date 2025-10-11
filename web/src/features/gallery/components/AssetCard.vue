<template>
  <div
    class="group relative w-full overflow-hidden rounded bg-background transition-all duration-200 select-none"
    :class="[
      {
        'ring-2 ring-primary ring-offset-2': isSelected,
        'ring-1 ring-border': isActive && !isSelected,
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
          <!-- 类型图标 -->
          <div class="flex items-center space-x-1">
            <span class="text-sm">
              {{ getTypeIcon(asset.type) }}
            </span>
            <span v-if="showTypeLabel" class="text-xs">{{ getTypeLabel(asset.type) }}</span>
          </div>

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

      <!-- 快速操作按钮（悬停时显示） -->
      <div
        class="absolute top-2 left-2 flex space-x-1 opacity-0 transition-opacity duration-200 group-hover:opacity-100"
        @click.stop
      >
        <button
          v-if="showQuickActions"
          class="flex h-6 w-6 items-center justify-center rounded-full bg-black/50 text-white transition-colors hover:bg-black/70"
          title="预览"
          @click="$emit('preview', asset)"
        >
          <svg class="h-3 w-3" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path
              stroke-linecap="round"
              stroke-linejoin="round"
              stroke-width="2"
              d="M15 12a3 3 0 11-6 0 3 3 0 016 0z"
            />
            <path
              stroke-linecap="round"
              stroke-linejoin="round"
              stroke-width="2"
              d="M2.458 12C3.732 7.943 7.523 5 12 5c4.478 0 8.268 2.943 9.542 7-1.274 4.057-5.064 7-9.542 7-4.477 0-8.268-2.943-9.542-7z"
            />
          </svg>
        </button>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed } from 'vue'
import { galleryApi } from '../api'
import type { Asset } from '../types'

// Props 定义
interface AssetCardProps {
  asset: Asset
  isSelected?: boolean
  isActive?: boolean
  showName?: boolean
  showSize?: boolean
  showTypeLabel?: boolean
  showQuickActions?: boolean
}

const props = withDefaults(defineProps<AssetCardProps>(), {
  isSelected: false,
  isActive: false,
  showName: true,
  showSize: false,
  showTypeLabel: false,
  showQuickActions: true,
})

// Emits 定义
const emit = defineEmits<{
  click: [asset: Asset, event: MouseEvent]
  doubleClick: [asset: Asset, event: MouseEvent]
  select: [assetId: number, selected: boolean, event: MouseEvent]
  contextMenu: [asset: Asset, event: MouseEvent]
  preview: [asset: Asset]
}>()

// 响应式状态
const isImageLoading = ref(true)
const imageError = ref(false)

// 缩略图URL - 直接从asset对象构建
const thumbnailUrl = computed(() => {
  return galleryApi.getAssetThumbnailUrl(props.asset)
})

// 事件处理
function handleClick(event: MouseEvent) {
  emit('click', props.asset, event)

  // 如果是 Ctrl/Cmd + 点击，切换选择状态
  if (event.ctrlKey || event.metaKey) {
    emit('select', props.asset.id, !props.isSelected, event)
  }
}

function handleDoubleClick(event: MouseEvent) {
  emit('doubleClick', props.asset, event)
}

function handleContextMenu(event: MouseEvent) {
  event.preventDefault()
  emit('contextMenu', props.asset, event)
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

// 工具函数
function getTypeIcon(type: Asset['type']): string {
  switch (type) {
    case 'photo':
      return '📷'
    case 'video':
      return '🎥'
    case 'live_photo':
      return '📸'
    default:
      return '📄'
  }
}

function getTypeLabel(type: Asset['type']): string {
  switch (type) {
    case 'photo':
      return '照片'
    case 'video':
      return '视频'
    case 'live_photo':
      return '实况'
    default:
      return '未知'
  }
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
