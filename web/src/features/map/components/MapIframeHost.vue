<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useGalleryStore } from '@/features/gallery/store'
import { useMapStore } from '@/features/map/store'
import { buildMapRuntimeScript } from '@/features/map/injection/mapRuntime'

const MAP_URL = 'https://myl.nuanpaper.com/tools/map'
const MAP_ORIGIN = 'https://myl.nuanpaper.com'

const route = useRoute()
const router = useRouter()
const galleryStore = useGalleryStore()
const mapStore = useMapStore()
const mapIframe = ref<HTMLIFrameElement | null>(null)

const isMapRoute = computed(() => route.name === 'map')

function syncMarkersToMap() {
  const contentWindow = mapIframe.value?.contentWindow
  if (!contentWindow) {
    return
  }

  if (import.meta.env.DEV) {
    injectDevScript()
    return
  }

  const markers = mapStore.markers.map((marker) => ({
    lat: marker.lat,
    lng: marker.lng,
    popupHtml: marker.popupHtml,
  }))

  contentWindow.postMessage(
    {
      action: 'SET_MARKERS',
      payload: {
        markers,
      },
    },
    MAP_ORIGIN
  )
}

function syncClusterOptionsToMap() {
  const contentWindow = mapIframe.value?.contentWindow
  if (!contentWindow || import.meta.env.DEV) {
    return
  }

  contentWindow.postMessage(
    {
      action: 'SET_CLUSTER_OPTIONS',
      payload: {
        ...mapStore.runtimeOptions,
      },
    },
    MAP_ORIGIN
  )
}

function syncRenderOptionsToMap() {
  const contentWindow = mapIframe.value?.contentWindow
  if (!contentWindow) {
    return
  }

  if (import.meta.env.DEV) {
    injectDevScript()
    return
  }

  const options = {
    mapBackgroundColor: mapStore.renderOptions.mapBackgroundColor,
    markerPinBackgroundUrl: mapStore.renderOptions.markerPinBackgroundUrl,
    markerIconUrl: mapStore.renderOptions.markerIconUrl,
    markerIconSize: mapStore.renderOptions.markerIconSize
      ? [...mapStore.renderOptions.markerIconSize]
      : undefined,
    markerIconAnchor: mapStore.renderOptions.markerIconAnchor
      ? [...mapStore.renderOptions.markerIconAnchor]
      : undefined,
    popupAnchor: mapStore.renderOptions.popupAnchor
      ? [...mapStore.renderOptions.popupAnchor]
      : undefined,
    openPopupOnHover: mapStore.renderOptions.openPopupOnHover,
    closePopupOnMouseOut: mapStore.renderOptions.closePopupOnMouseOut,
    popupOpenDelayMs: mapStore.renderOptions.popupOpenDelayMs,
    popupCloseDelayMs: mapStore.renderOptions.popupCloseDelayMs,
    keepPopupVisibleOnHover: mapStore.renderOptions.keepPopupVisibleOnHover,
  }

  contentWindow.postMessage(
    {
      action: 'SET_RENDER_OPTIONS',
      payload: options,
    },
    MAP_ORIGIN
  )
}

function injectDevScript() {
  if (!import.meta.env.DEV) {
    return
  }

  const contentWindow = mapIframe.value?.contentWindow
  if (!contentWindow) {
    return
  }

  const script = buildMapRuntimeScript({
    markers: mapStore.markers.map((marker) => ({ ...marker })),
    renderOptions: {
      ...mapStore.renderOptions,
      markerIconSize: mapStore.renderOptions.markerIconSize
        ? [...mapStore.renderOptions.markerIconSize]
        : undefined,
      markerIconAnchor: mapStore.renderOptions.markerIconAnchor
        ? [...mapStore.renderOptions.markerIconAnchor]
        : undefined,
      popupAnchor: mapStore.renderOptions.popupAnchor
        ? [...mapStore.renderOptions.popupAnchor]
        : undefined,
    },
    runtimeOptions: {
      ...mapStore.runtimeOptions,
    },
  })

  contentWindow.postMessage(
    {
      action: 'EVAL_SCRIPT',
      payload: {
        script,
      },
    },
    MAP_ORIGIN
  )
}

function handleIframeLoad() {
  injectDevScript()
  syncClusterOptionsToMap()
  syncRenderOptionsToMap()
  syncMarkersToMap()
}

type OpenGalleryAssetMessage = {
  action: 'SPINNING_MOMO_OPEN_GALLERY_ASSET'
  payload?: {
    assetId?: number
    assetIndex?: number
  }
}

type SetMarkersVisibleMessage = {
  action: 'SPINNING_MOMO_SET_MARKERS_VISIBLE'
  payload?: {
    markersVisible?: boolean
  }
}

async function handleMapMessage(event: MessageEvent<unknown>) {
  if (event.origin !== MAP_ORIGIN) {
    return
  }

  const data = event.data as OpenGalleryAssetMessage | SetMarkersVisibleMessage
  if (!data) {
    return
  }

  if (data.action === 'SPINNING_MOMO_SET_MARKERS_VISIBLE') {
    const markersVisible = data.payload?.markersVisible
    if (typeof markersVisible !== 'boolean') {
      return
    }

    mapStore.setRuntimeOptions({ markersVisible })
    return
  }

  if (data.action !== 'SPINNING_MOMO_OPEN_GALLERY_ASSET') {
    return
  }

  const assetId = Number(data.payload?.assetId)
  if (!Number.isFinite(assetId)) {
    return
  }

  const assetIndex = Number(data.payload?.assetIndex)
  const normalizedAssetIndex = Number.isFinite(assetIndex) ? assetIndex : 0

  galleryStore.setActiveAsset(assetId, normalizedAssetIndex)

  galleryStore.openLightbox()

  try {
    await router.push({
      name: 'gallery',
    })
  } catch {
    galleryStore.closeLightbox()
  }
}

watch(
  () => mapStore.markers,
  () => {
    syncMarkersToMap()
  },
  { deep: true }
)

watch(
  () => mapStore.renderOptions,
  () => {
    syncRenderOptionsToMap()
    syncMarkersToMap()
  },
  { deep: true }
)

watch(
  () => mapStore.runtimeOptions,
  () => {
    injectDevScript()
    syncClusterOptionsToMap()
  },
  { deep: true }
)

watch(isMapRoute, (visible) => {
  if (visible) {
    injectDevScript()
    syncClusterOptionsToMap()
    syncRenderOptionsToMap()
    syncMarkersToMap()
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
