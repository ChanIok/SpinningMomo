<script setup lang="ts">
import type { Screenshot } from '@/types/screenshot'

const props = defineProps<{
  screenshots: Screenshot[]
  loading: boolean
  hasMore: boolean
  selectedId?: number // 新增：当前选中的截图ID
}>()

const emit = defineEmits<{
  (e: 'load-more'): void
  (e: 'screenshot-click', id: number): void // 新增：点击截图事件
}>()

function formatFileSize(bytes: number): string {
  if (bytes === 0) return '0 B'
  const k = 1024
  const sizes = ['B', 'KB', 'MB', 'GB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
}

function formatDate(timestamp: number): string {
  return new Date(timestamp).toLocaleString()
}

// 处理截图点击
function handleScreenshotClick(id: number) {
  emit('screenshot-click', id)
}
</script>

<template>
  <div class="p-4">
    <!-- 列表容器 -->
    <div class="divide-y divide-gray-200">
      <!-- 列表项 -->
      <div
        v-for="screenshot in props.screenshots"
        :key="screenshot.id"
        class="py-4 first:pt-0 last:pb-0 cursor-pointer transition-colors"
        :class="{
          'bg-gray-100 dark:bg-gray-800': props.selectedId === screenshot.id
        }"
        @click="handleScreenshotClick(screenshot.id)"
      >
        <div class="flex gap-4 items-center">
          <!-- 缩略图 -->
          <div class="w-[120px] h-[68px] overflow-hidden rounded flex-shrink-0 bg-background">
            <img
              :src="screenshot.thumbnailPath"
              :alt="screenshot.filename"
              loading="lazy"
              class="w-full h-full object-cover"
            />
          </div>
          <!-- 信息区域 -->
          <div class="flex-1 min-w-0">
            <h3 class="m-0 mb-2 text-base font-medium text-gray-900 dark:text-gray-100 truncate">
              {{ screenshot.filename }}
            </h3>
            <div class="flex gap-4 text-sm text-gray-500 dark:text-gray-400">
              <span>{{ formatFileSize(screenshot.file_size) }}</span>
              <span>{{ screenshot.width }} x {{ screenshot.height }}</span>
              <span>{{ formatDate(screenshot.created_at) }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- 加载更多区域 -->
    <div v-if="props.loading || props.hasMore" class="flex justify-center py-6">
      <!-- 加载中状态 -->
      <div
        v-if="props.loading"
        class="inline-block animate-spin rounded-full h-5 w-5 border-2 border-primary border-t-transparent"
      ></div>
      <!-- 加载更多按钮 -->
      <button
        v-else-if="props.hasMore"
        class="px-4 py-2 rounded bg-primary text-white cursor-pointer transition-opacity duration-200 hover:opacity-90"
        @click="emit('load-more')"
      >
        加载更多
      </button>
    </div>
  </div>
</template>
