<script setup lang="ts">
import { onMounted } from 'vue'
import ApiPlaygroundPage from './ApiPlaygroundPage.vue'

onMounted(() => {
  document.title = 'API 测试工具'
})
</script>

<template>
  <div class="flex h-full flex-col bg-background">
    <div class="flex items-center gap-6 border-b p-4">
      <div class="flex-1">
        <p class="text-muted-foreground">开发者 API 测试工具</p>
      </div>
    </div>

    <div class="flex flex-1 flex-col overflow-hidden">
      <ApiPlaygroundPage />
    </div>
  </div>
</template>
