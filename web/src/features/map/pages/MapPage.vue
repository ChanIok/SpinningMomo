<script setup lang="ts">
import { onMounted } from 'vue'
import { useMapScene } from '@/features/map/composables/useMapScene'
import { useMapStore } from '@/features/map/store'

defineOptions({
  name: 'MapPage',
})

const mapStore = useMapStore()
const { mapPoints, initializeMapDefaults } = useMapScene()

onMounted(() => {
  initializeMapDefaults()
})
</script>

<template>
  <div class="relative flex h-full w-full flex-col bg-background pt-[var(--titlebar-height,20px)]">
    <div class="z-10 flex h-12 w-full shrink-0 items-center border-b bg-background px-4">
      <h1 class="text-base font-semibold">官方地图</h1>
      <div class="ml-auto text-sm text-muted-foreground">
        <span v-if="mapStore.isLoading">正在同步照片坐标…</span>
        <span v-else>当前筛选下 {{ mapPoints.length }} 张照片</span>
      </div>
    </div>
    <div class="relative w-full flex-1"></div>
  </div>
</template>
