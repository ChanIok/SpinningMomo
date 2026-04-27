<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useGalleryStore } from '@/features/gallery/store'
import { MAP_URL } from '@/features/map/bridge/protocol'
import { useMapBridge } from '@/features/map/composables/useMapBridge'
import {
  flushMapRuntimeToIframe,
  registerMapIframeFlush,
} from '@/features/map/composables/mapIframeRuntime'
import { normalizeOfficialWorldId } from '@/features/map/domain/officialWorldId'
import { useMapStore } from '@/features/map/store'

const route = useRoute()
const router = useRouter()
const galleryStore = useGalleryStore()
const mapStore = useMapStore()
const mapIframe = ref<HTMLIFrameElement | null>(null)

const isMapRoute = computed(() => route.name === 'map')
const mapIframeSrc = ref(MAP_URL)
const { postRuntimeSync, handleMapMessage } = useMapBridge({
  mapIframe,
  mapStore,
  galleryStore,
  router,
})

function applyPendingFocusRequest() {
  const target = mapStore.pendingFocusRequest
  if (!target) {
    return
  }

  const nextWorldId = normalizeOfficialWorldId(target.worldId)
  const currentWorldId = normalizeOfficialWorldId(mapStore.runtimeOptions.currentWorldId)

  if (nextWorldId && nextWorldId !== currentWorldId) {
    const url = new URL(MAP_URL)
    url.searchParams.set('worldId', nextWorldId)
    const nextSrc = url.toString()
    if (mapIframeSrc.value !== nextSrc) {
      mapIframeSrc.value = nextSrc
    }
    return
  }

  mapStore.consumePendingFocusRequest()
  mapStore.patchRuntimeOptions({
    focusedAssetId: target.assetId,
    focusRequestId: target.requestId,
    markersVisible: true,
    currentWorldId: nextWorldId ?? currentWorldId,
  })
  if (mapStore.iframeSessionReady) {
    flushMapRuntimeToIframe()
  }
}

watch(
  () => [
    mapStore.pendingFocusRequest,
    mapStore.iframeSessionReady,
    mapStore.runtimeOptions.currentWorldId,
  ],
  () => {
    applyPendingFocusRequest()
  }
)

watch(mapIframeSrc, (nextSrc, previousSrc) => {
  if (nextSrc === previousSrc) {
    return
  }

  mapStore.resetIframeSession()
  mapStore.replaceMarkers([])
  mapStore.patchRuntimeOptions({
    currentWorldId: undefined,
  })
})

watch(isMapRoute, (visible) => {
  if (visible && mapStore.iframeSessionReady) {
    flushMapRuntimeToIframe()
  }
})

onMounted(() => {
  registerMapIframeFlush(() => {
    postRuntimeSync()
  })
  window.addEventListener('message', handleMapMessage)
})

onUnmounted(() => {
  window.removeEventListener('message', handleMapMessage)
})
</script>

<template>
  <div v-show="isMapRoute" class="absolute inset-x-0 top-0 bottom-0 z-0">
    <iframe
      ref="mapIframe"
      :src="mapIframeSrc"
      class="absolute inset-0 h-full w-full border-none"
      allowfullscreen
    ></iframe>
  </div>
</template>
