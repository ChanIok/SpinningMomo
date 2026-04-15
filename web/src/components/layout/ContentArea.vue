<script setup lang="ts">
import MapIframeHost from '@/features/map/components/MapIframeHost.vue'
</script>

<template>
  <div class="flex h-full w-full flex-1 flex-col">
    <main class="route-scene relative h-full flex-1 overflow-auto">
      <router-view />
      <MapIframeHost />
    </main>
  </div>
</template>
