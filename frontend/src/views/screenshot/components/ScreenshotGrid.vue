<script setup lang="ts">
import type { Screenshot } from '@/types/screenshot'
import ScreenshotCard from '@/components/screenshot/ScreenshotCard.vue'

const props = defineProps<{
  screenshots: Screenshot[]
  loading: boolean
  hasMore: boolean
  selectedId?: number
}>()

const emit = defineEmits<{
  (e: 'load-more'): void
  (e: 'screenshot-click', screenshot: Screenshot): void
}>()

function handleScreenshotClick(screenshot: Screenshot) {
  emit('screenshot-click', screenshot)
}
</script>

<template>
  <div class="p-3">
    <!-- 使用Tailwind网格布局 -->
    <div
      class="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5 xl:grid-cols-6 2xl:grid-cols-8 gap-3"
    >
      <screenshot-card
        v-for="screenshot in props.screenshots"
        :key="screenshot.id"
        :screenshot="screenshot"
        :selected="props.selectedId === screenshot.id"
        @click="handleScreenshotClick"
      />
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
