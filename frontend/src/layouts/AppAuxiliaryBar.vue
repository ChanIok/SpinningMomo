<template>
  <div id="app-auxiliary-bar" class="w-64 border-l border-gray-200 dark:border-gray-700 overflow-y-auto">
    <!-- 辅助栏内容 -->
    <div class="p-4">
      <h2 class="text-lg font-semibold mb-4">辅助信息</h2>
      <!-- 这里可以放置辅助信息，如属性面板、大纲视图等 -->
      <div class="space-y-2">
        <div class="p-2 hover:bg-gray-200 dark:hover:bg-gray-800 rounded cursor-pointer">
          属性面板
        </div>
        <div class="p-2 hover:bg-gray-200 dark:hover:bg-gray-800 rounded cursor-pointer">
          大纲视图
        </div>
        <div class="p-2 hover:bg-gray-200 dark:hover:bg-gray-800 rounded cursor-pointer">
          帮助信息
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
// 辅助栏逻辑
</script>

<style scoped>
/* 使用TailwindCSS类，无需额外样式 */
</style>