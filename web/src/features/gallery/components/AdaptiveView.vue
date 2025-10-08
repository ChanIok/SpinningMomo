<script setup lang="ts">
import { useGalleryView } from '../composables'

const galleryView = useGalleryView()
</script>

<template>
  <div class="h-full overflow-auto p-6">
    <div class="text-center text-muted-foreground">
      <p class="mb-2">自适应视图</p>
      <p class="text-sm">暂未实现</p>
      <p class="mt-4 text-xs">共 {{ galleryView.sortedAssets.value.length }} 项</p>
    </div>
  </div>
</template>