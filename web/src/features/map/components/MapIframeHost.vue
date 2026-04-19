<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useGalleryStore } from '@/features/gallery/store'
import { MAP_URL } from '@/features/map/bridge/protocol'
import { useMapBridge } from '@/features/map/composables/useMapBridge'
import { useMapStore } from '@/features/map/store'

const route = useRoute()
const router = useRouter()
const galleryStore = useGalleryStore()
const mapStore = useMapStore()
const mapIframe = ref<HTMLIFrameElement | null>(null)

const isMapRoute = computed(() => route.name === 'map')
const { postRuntimeSync, handleMapMessage } = useMapBridge({
  mapIframe,
  mapStore,
  galleryStore,
  router,
})

function handleIframeLoad() {
  postRuntimeSync()
}

watch(
  () => mapStore.markers,
  () => {
    postRuntimeSync()
  },
  { deep: true }
)

watch(
  () => mapStore.renderOptions,
  () => {
    postRuntimeSync()
  },
  { deep: true }
)

watch(
  () => mapStore.runtimeOptions,
  () => {
    postRuntimeSync()
  },
  { deep: true }
)

watch(isMapRoute, (visible) => {
  if (visible) {
    postRuntimeSync()
  }
})

onMounted(() => {
  window.addEventListener('message', handleMapMessage)
})

onUnmounted(() => {
  window.removeEventListener('message', handleMapMessage)
})
</script>

<template>
  <div
    v-show="isMapRoute"
    class="absolute inset-x-0 top-[calc(var(--titlebar-height,20px)+3rem)] bottom-0 z-0"
  >
    <iframe
      ref="mapIframe"
      :src="MAP_URL"
      class="absolute inset-0 h-full w-full border-none"
      allowfullscreen
      @load="handleIframeLoad"
    ></iframe>
  </div>
</template>
