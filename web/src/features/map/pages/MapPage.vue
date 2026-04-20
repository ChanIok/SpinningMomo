<script setup lang="ts">
import { onMounted } from 'vue'
import { useMapScene } from '@/features/map/composables/useMapScene'

defineOptions({
  name: 'MapPage',
})

const { initializeMapDefaults } = useMapScene()

onMounted(() => {
  initializeMapDefaults()
})
</script>

<template>
  <div class="relative flex h-full w-full flex-col bg-background">
    <div class="relative w-full flex-1"></div>
  </div>
</template>
