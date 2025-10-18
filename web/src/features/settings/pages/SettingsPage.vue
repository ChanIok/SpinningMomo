<script setup lang="ts">
// 临时的设置页面组件，用于测试布局
import { useRouter } from 'vue-router'

const router = useRouter()

const navigateToPlayground = () => {
  router.push('/playground')
}
</script>

<template>
  <div class="flex h-full items-center justify-center">
    <div class="text-center">
      <h1 class="mb-4 text-4xl font-bold">设置</h1>
      <p class="text-muted-foreground">这是设置页面 - Settings Page</p>
      <button
        @click="navigateToPlayground"
        class="mt-6 rounded bg-blue-500 px-4 py-2 text-white transition-colors hover:bg-blue-600"
      >
        前往开发工具
      </button>
    </div>
  </div>
</template>
