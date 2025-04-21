<script setup lang="ts">
import { GridOutline, ListOutline } from '@vicons/ionicons5'
import { useUIStore } from '@/stores'

// 使用UI store
const uiStore = useUIStore()
</script>

<template>
  <div class="flex items-center">
    <div class="flex gap-1">
      <button
        class="p-1.5 rounded-md transition-colors"
        :class="uiStore.viewMode === 'grid' ? 'bg-blue-100 text-blue-600 dark:bg-blue-900/50 dark:text-blue-400' : 'hover:bg-gray-100 dark:hover:bg-gray-700'"
        @click="uiStore.setViewMode('grid')"
      >
        <grid-outline class="w-5 h-5" />
      </button>
      <button
        class="p-1.5 rounded-md transition-colors"
        :class="uiStore.viewMode === 'list' ? 'bg-blue-100 text-blue-600 dark:bg-blue-900/50 dark:text-blue-400' : 'hover:bg-gray-100 dark:hover:bg-gray-700'"
        @click="uiStore.setViewMode('list')"
      >
        <list-outline class="w-5 h-5" />
      </button>
    </div>
  </div>
</template>

<style scoped>
/* 所有样式已通过Tailwind类实现 */
</style>