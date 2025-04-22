<script setup lang="ts">
import { GridOutline, ListOutline } from '@vicons/ionicons5'
import { useUIStore } from '@/stores'
import { useScreenshotListStore } from '@/stores/screenshot-list'

// 使用store
const uiStore = useUIStore()
const screenshotListStore = useScreenshotListStore()

// 切换到网格视图
function switchToGridView() {
  uiStore.setViewMode('grid')
}

// 切换到列表视图
function switchToListView() {
  uiStore.setViewMode('list')

  // 如果没有选中的截图且有截图数据，默认选中第一张
  if (screenshotListStore.currentIndex === -1 && screenshotListStore.screenshots.length > 0) {
    screenshotListStore.setCurrentIndex(0)
  }
}
</script>

<template>
  <div class="flex items-center">
    <div class="flex gap-1">
      <button
        class="p-1.5 rounded-md transition-colors"
        :class="
          uiStore.viewMode === 'grid'
            ? 'bg-gray-100 text-gray-600 dark:bg-gray-900/50 dark:text-gray-400'
            : 'hover:bg-gray-100 dark:hover:bg-gray-700'
        "
        @click="switchToGridView()"
      >
        <grid-outline class="w-5 h-5" />
      </button>
      <button
        class="p-1.5 rounded-md transition-colors"
        :class="
          uiStore.viewMode === 'list'
            ? 'bg-gray-100 text-gray-600 dark:bg-gray-900/50 dark:text-gray-400'
            : 'hover:bg-gray-100 dark:hover:bg-gray-700'
        "
        @click="switchToListView()"
      >
        <list-outline class="w-5 h-5" />
      </button>
    </div>
  </div>
</template>