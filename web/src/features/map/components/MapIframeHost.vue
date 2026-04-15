<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { useRoute } from 'vue-router'
import { useMapStore } from '@/features/map/store'
import { buildMapRuntimeScript } from '@/features/map/injection/mapRuntime'

const MAP_URL = 'https://myl.nuanpaper.com/tools/map'
const MAP_ORIGIN = 'https://myl.nuanpaper.com'

const route = useRoute()
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
        clusterEnabled: mapStore.runtimeOptions.clusterEnabled,
        clusterRadius: mapStore.runtimeOptions.clusterRadius,
        hoverCardEnabled: mapStore.runtimeOptions.hoverCardEnabled,
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
