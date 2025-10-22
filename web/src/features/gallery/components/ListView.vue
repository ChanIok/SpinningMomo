<script setup lang="ts">
import { useGalleryStore } from '../store'

const store = useGalleryStore()
</script>

<template>
  <div class="h-full overflow-auto p-6">
    <div class="text-center text-muted-foreground">
      <p class="mb-2">列表视图</p>
      <p class="text-sm">暂未实现</p>
      <p class="mt-4 text-xs">共 {{ store.totalCount }} 项</p>
    </div>
  </div>
</template>
