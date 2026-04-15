<script setup lang="ts">
import { ref, watch } from 'vue'
import { useI18n } from '@/composables/useI18n'
import type { Locale } from '@/core/i18n/types'
import { queryPhotoMapPoints } from '@/features/gallery/api'
import { toQueryAssetsFilters } from '@/features/gallery/queryFilters'
import { useGalleryStore } from '@/features/gallery/store'
import type { PhotoMapPoint } from '@/features/gallery/types'
import { useMapStore, type MapMarker, type MapRenderOptions } from '@/features/map/store'

defineOptions({
  name: 'MapPage',
})

const { t, locale } = useI18n()
const galleryStore = useGalleryStore()
const mapStore = useMapStore()
const mapPoints = ref<PhotoMapPoint[]>([])

// 模型A：mapX = kx * gameX + bx，mapY = ky * gameY + by
const MAP_X_SCALE = 1.00010613
const MAP_X_BIAS = 756015.585
const MAP_Y_SCALE = 1.00001142
const MAP_Y_BIAS = 392339.507
const DEFAULT_MAP_RENDER_OPTIONS: MapRenderOptions = {
  markerPinBackgroundUrl:
    'https://assets.papegames.com/nikkiweb/infinitynikki/infinitynikki-map/img/58ca045d59db0f9cd8ad.png',
  markerIconUrl: 'https://webstatic.papegames.com/a6f47b49876cbaff/images/bg/EzoioGtm0TN1V9Ua.png',
  markerIconSize: [32, 32],
  markerIconAnchor: [14, 14],
  popupAnchor: [0, -14],
  openPopupOnHover: true,
  closePopupOnMouseOut: true,
  popupOpenDelayMs: 180,
  popupCloseDelayMs: 260,
  keepPopupVisibleOnHover: true,
}

mapStore.setRenderOptions(DEFAULT_MAP_RENDER_OPTIONS)

function syncMapRuntimeI18n() {
  mapStore.setRuntimeOptions({
    clusterTitleTemplate: t('map.cluster.title'),
  })
}

mapStore.setRuntimeOptions({
  clusterEnabled: true,
  clusterRadius: 44,
  hoverCardEnabled: true,
  thumbnailBaseUrl: 'http://127.0.0.1:51206',
  clusterTitleTemplate: t('map.cluster.title'),
})

function transformGameToMapCoordinates(point: PhotoMapPoint): { lat: number; lng: number } {
  const mapX = point.nikkiLocX * MAP_X_SCALE + MAP_X_BIAS
  const mapY = point.nikkiLocY * MAP_Y_SCALE + MAP_Y_BIAS

  return {
    // 官方地图经纬度约定与采集数据轴向相反，这里交换 x/y。
    lat: mapY,
    lng: mapX,
  }
}

function escapeHtml(value: string): string {
  return value
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;')
}

const FILENAME_DATE_PREFIX_RE = /^(\d{4})_(\d{2})_(\d{2})_(\d{2})_(\d{2})_(\d{2})/

function formatPopupTitleFromFilename(fileName: string, currentLocale: Locale): string {
  const base = fileName.replace(/^.*[/\\]/, '')
  const m = base.match(FILENAME_DATE_PREFIX_RE)
  if (!m) {
    return t('map.popup.fallbackTitle')
  }
  const y = Number(m[1])
  const mo = Number(m[2])
  const d = Number(m[3])
  if (!Number.isFinite(y) || !Number.isFinite(mo) || !Number.isFinite(d)) {
    return t('map.popup.fallbackTitle')
  }
  if (currentLocale === 'zh-CN') {
    return `${y}年${mo}月${d}日`
  }
  try {
    return new Date(y, mo - 1, d).toLocaleDateString('en-US', {
      year: 'numeric',
      month: 'long',
      day: 'numeric',
    })
  } catch {
    return t('map.popup.fallbackTitle')
  }
}

function getThumbnailUrl(point: PhotoMapPoint): string {
  if (!point.hash) {
    return ''
  }

  const prefix1 = point.hash.slice(0, 2)
  const prefix2 = point.hash.slice(2, 4)
  const relativePath = `${prefix1}/${prefix2}/${point.hash}.webp`

  const thumbnailBaseUrl = mapStore.runtimeOptions.thumbnailBaseUrl.replace(/\/+$/, '')
  return `${thumbnailBaseUrl}/static/assets/thumbnails/${relativePath}`
}

function buildPopupHtml(point: PhotoMapPoint): string {
  const title = escapeHtml(formatPopupTitleFromFilename(point.name, locale.value))
  const thumbnailUrl = getThumbnailUrl(point)
  const clickableAttr = Number.isFinite(point.assetId)
    ? ` data-sm-open-asset-id="${point.assetId}"`
    : ''
  const thumbnailSection = thumbnailUrl
    ? `<div style="margin-top: 8px;">
            <img
              src="${thumbnailUrl}"
              alt="${title}"
              loading="lazy"
              style="display: block; width: 180px; max-width: 100%; border-radius: 6px; background: #f2f2f2;"
              onerror="this.style.display='none'; const fallback = this.nextElementSibling; if (fallback) fallback.style.display='block';"
            />
            <div style="display: none; font-size: 12px; color: #888;">缩略图加载失败</div>
          </div>`
    : ''

  return `<div${clickableAttr} style="min-width: 180px; line-height: 1.5; cursor: pointer;">
            <div style="font-size: 13px; font-weight: 600; margin-bottom: 4px; color: rgb(123, 93, 74); font-family: 'Helvetica Neue', Arial, Helvetica, sans-serif;">${title}</div>
            ${thumbnailSection}
          </div>`
}

function buildMapMarkers(points: PhotoMapPoint[]): MapMarker[] {
  return points.map((point) => {
    const { lat, lng } = transformGameToMapCoordinates(point)

    return {
      assetId: point.assetId,
      name: point.name,
      lat,
      lng,
      popupHtml: buildPopupHtml(point),
      thumbnailUrl: getThumbnailUrl(point),
      fileCreatedAt: point.fileCreatedAt,
    }
  })
}

async function loadMapPoints() {
  mapStore.isLoading = true

  try {
    const filters = toQueryAssetsFilters(galleryStore.filter, galleryStore.includeSubfolders)
    mapPoints.value = await queryPhotoMapPoints({ filters })
    mapStore.setMarkers(buildMapMarkers(mapPoints.value))
  } catch (error) {
    console.error('Failed to load map points:', error)
    mapPoints.value = []
    mapStore.setMarkers([])
  } finally {
    mapStore.isLoading = false
  }
}

watch(
  () => [galleryStore.filter, galleryStore.includeSubfolders],
  async () => {
    await loadMapPoints()
  },
  { deep: true, immediate: true }
)

watch(locale, () => {
  syncMapRuntimeI18n()
  void loadMapPoints()
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
