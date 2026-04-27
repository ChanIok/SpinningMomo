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
const forcedWorldId = computed(() => normalizeOfficialWorldId(readQueryValue(route.query.worldId)))
const requestedMapIframeSrc = computed(() => {
  const worldId = forcedWorldId.value

  if (!worldId) {
    return MAP_URL
  }

  const url = new URL(MAP_URL)
  url.searchParams.set('worldId', worldId)
  return url.toString()
})
const mapIframeSrc = ref(MAP_URL)
const mapFocusTarget = computed(() => {
  const assetId = readPositiveIntegerQueryValue(route.query.pinAssetId)
  if (!assetId) {
    return null
  }

  return {
    assetId,
    requestId: readPositiveIntegerQueryValue(route.query.focusRequestId) ?? assetId,
  }
})
const { postRuntimeSync, handleMapMessage } = useMapBridge({
  mapIframe,
  mapStore,
  galleryStore,
  router,
  forcedWorldId,
})

function readQueryValue(value: unknown): string | undefined {
  const raw = Array.isArray(value) ? value[0] : value
  if (typeof raw !== 'string') {
    return undefined
  }

  const normalized = raw.trim()
  return normalized ? normalized : undefined
}

function readPositiveIntegerQueryValue(value: unknown): number | undefined {
  const normalized = readQueryValue(value)
  if (!normalized) {
    return undefined
  }

  const parsed = Number(normalized)
  return Number.isInteger(parsed) && parsed > 0 ? parsed : undefined
}

watch(
  [isMapRoute, requestedMapIframeSrc],
  ([visible, nextSrc]) => {
    if (!visible || mapIframeSrc.value === nextSrc) {
      return
    }

    mapIframeSrc.value = nextSrc
  },
  { immediate: true }
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

watch(
  mapFocusTarget,
  (target) => {
    if (!target) {
      mapStore.patchRuntimeOptions({
        focusedAssetId: undefined,
        focusRequestId: undefined,
      })
      if (mapStore.iframeSessionReady) {
        flushMapRuntimeToIframe()
      }
      return
    }

    mapStore.patchRuntimeOptions({
      focusedAssetId: target.assetId,
      focusRequestId: target.requestId,
      markersVisible: true,
    })

    if (mapStore.iframeSessionReady) {
      flushMapRuntimeToIframe()
    }
  },
  { immediate: true }
)

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
