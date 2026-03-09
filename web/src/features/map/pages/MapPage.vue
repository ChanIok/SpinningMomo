<script setup lang="ts">
import { ref, watch } from 'vue'
import { queryPhotoMapPoints } from '@/features/gallery/api'
import { toQueryAssetsFilters } from '@/features/gallery/queryFilters'
import { useGalleryStore } from '@/features/gallery/store'
import type { PhotoMapPoint } from '@/features/gallery/types'

const MAP_URL = 'https://myl.nuanpaper.com/tools/map'
const MAP_ORIGIN = 'https://myl.nuanpaper.com'

const galleryStore = useGalleryStore()
const mapIframe = ref<HTMLIFrameElement | null>(null)
const mapPoints = ref<PhotoMapPoint[]>([])
const isLoading = ref(false)

function escapeHtml(value: string): string {
  return value
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;')
}

function formatCoordinate(value: number | undefined): string {
  if (value === undefined) {
    return '-'
  }

  return Number(value)
    .toFixed(3)
    .replace(/\.?0+$/, '')
}

function buildPopupHtml(point: PhotoMapPoint): string {
  const name = escapeHtml(point.name)
  const x = formatCoordinate(point.nikkiLocX)
  const y = formatCoordinate(point.nikkiLocY)
  const z = formatCoordinate(point.nikkiLocZ)

  return `<div style="min-width: 180px; line-height: 1.5;">
            <div style="font-size: 13px; font-weight: 600; margin-bottom: 4px;">${name}</div>
            <div style="font-size: 12px; color: #666;">坐标: (${x}, ${y}, ${z})</div>
          </div>`
}

function syncMarkersToMap() {
  const contentWindow = mapIframe.value?.contentWindow
  if (!contentWindow) {
    return
  }

  contentWindow.postMessage(
    {
      action: 'SET_MARKERS',
      payload: {
        markers: mapPoints.value.map((point) => ({
          lat: point.nikkiLocX,
          lng: point.nikkiLocY,
          popupHtml: buildPopupHtml(point),
        })),
      },
    },
    MAP_ORIGIN
  )
}

async function loadMapPoints() {
  isLoading.value = true

  try {
    const filters = toQueryAssetsFilters(galleryStore.filter, galleryStore.includeSubfolders)
    mapPoints.value = await queryPhotoMapPoints({ filters })
    syncMarkersToMap()
  } catch (error) {
    console.error('Failed to load map points:', error)
    mapPoints.value = []
    syncMarkersToMap()
  } finally {
    isLoading.value = false
  }
}

function handleIframeLoad() {
  syncMarkersToMap()
}

watch(
  () => [galleryStore.filter, galleryStore.includeSubfolders],
  async () => {
    await loadMapPoints()
  },
  { deep: true, immediate: true }
)
</script>

<template>
  <div class="relative flex h-full w-full flex-col bg-background pt-[var(--titlebar-height,20px)]">
    <div class="z-10 flex h-12 w-full shrink-0 items-center border-b bg-background px-4">
      <h1 class="text-base font-semibold">官方地图</h1>
      <div class="ml-auto text-sm text-muted-foreground">
        <span v-if="isLoading">正在同步照片坐标…</span>
        <span v-else>当前筛选下 {{ mapPoints.length }} 张照片</span>
      </div>
    </div>

    <div class="relative w-full flex-1">
      <iframe
        ref="mapIframe"
        :src="MAP_URL"
        class="absolute inset-0 h-full w-full border-none"
        allowfullscreen
        @load="handleIframeLoad"
      ></iframe>
    </div>
  </div>
</template>
