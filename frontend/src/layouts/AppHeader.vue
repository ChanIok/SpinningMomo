<script setup lang="ts">
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import ViewToolbar from '@/views/screenshot/components/ViewToolbar.vue'

const route = useRoute()

// 检查当前路由是否为截图相关页面
const isScreenshotRoute = computed(() => {
  return route.path.includes('/screenshot')
})
</script>

<template>
  <header>
    <div class="mx-4 flex justify-between items-center h-12 overflow-hidden">
      <div class="flex items-center">
        <h1 class="text-xl font-semibold text-gray-800 dark:text-white">SpinningMomo</h1>
      </div>

      <!-- 根据当前路由决定是否显示ViewToolbar -->
      <div v-if="isScreenshotRoute" class="header-toolbar">
        <view-toolbar />
      </div>
    </div>
  </header>
</template>
