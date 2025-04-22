<script setup lang="ts">
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import { useUIStore } from '@/stores'
import { useScreenshotListStore } from '@/stores/screenshot-list'
import ScreenshotList from '@/views/screenshot/components/ScreenshotList.vue'

const route = useRoute()
const uiStore = useUIStore()
const screenshotListStore = useScreenshotListStore()

// 检查当前路由是否为截图相关页面
const isScreenshotRoute = computed(() => {
  return route.path.includes('/screenshot') ||
         route.path === '/screenshots' ||
         route.path.includes('/albums/') ||
         route.path.includes('/calendar/')
})

// 是否显示截图列表
const showScreenshotList = computed(() => {
  return isScreenshotRoute.value && uiStore.viewMode === 'list'
})

// 处理列表项点击
function handleScreenshotClick(id: number) {
  // 发出自定义事件，通知父组件或通过事件总线通知其他组件
  const index = screenshotListStore.screenshots.findIndex(s => s.id === id)
  if (index !== -1) {
    // 设置当前选中的截图索引
    screenshotListStore.setCurrentIndex(index)
  }
}
</script>

<template>
  <div id="app-side-bar" class="flex-shrink-0 overflow-y-auto">
    <!-- 截图列表视图 -->
    <div v-if="showScreenshotList" class="min-w-64 h-full bg-white mr-2 overflow-auto rounded-lg">
      <div class="p-4 border-b border-gray-200 dark:border-gray-700">
        <h2 class="text-lg font-semibold">截图列表</h2>
      </div>
      <screenshot-list
        :screenshots="screenshotListStore.screenshots"
        :loading="screenshotListStore.loading"
        :has-more="screenshotListStore.hasMore"
        :selected-id="screenshotListStore.currentScreenshotId"
        @load-more="screenshotListStore.loadScreenshots()"
        @screenshot-click="handleScreenshotClick"
      />
    </div>
  </div>
</template>

<style scoped>
/* 使用TailwindCSS类，无需额外样式 */
</style>
