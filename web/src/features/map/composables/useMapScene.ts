import { computed, ref, watch } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { toQueryAssetsFilters } from '@/features/gallery/queryFilters'
import { useGalleryStore } from '@/features/gallery/store'
import type { PhotoMapPoint } from '@/features/gallery/types'
import { queryPhotoMapPoints } from '@/features/map/api'
import {
  createDefaultMapRenderOptions,
  createDefaultMapRuntimeOptions,
} from '@/features/map/domain/defaults'
import { toMapMarkers } from '@/features/map/domain/markerMapper'
import { stripOfficialWorldVersion } from '@/features/map/domain/officialWorldId'
import { flushMapRuntimeToIframe } from '@/features/map/composables/mapIframeRuntime'
import { useMapStore } from '@/features/map/store'

export function useMapScene() {
  const { t, locale } = useI18n()
  const galleryStore = useGalleryStore()
  const mapStore = useMapStore()
  const mapPoints = ref<PhotoMapPoint[]>([])
  let loadSequence = 0

  function getCurrentWorldId(): string {
    return String(mapStore.runtimeOptions.currentWorldId ?? '').trim()
  }

  function syncMarkersFromMapPoints() {
    mapStore.replaceMarkers(
      toMapMarkers(mapPoints.value, {
        locale: locale.value,
        thumbnailBaseUrl: mapStore.runtimeOptions.thumbnailBaseUrl,
        cardTitleFallback: t('map.popup.fallbackTitle'),
        worldId: getCurrentWorldId(),
      })
    )
  }

  function syncFilterCountCard(loading: boolean) {
    const currentWorldId = getCurrentWorldId()
    const countText = loading
      ? '正在同步照片坐标…'
      : currentWorldId
        ? `当前区域下 ${mapPoints.value.length} 张照片`
        : '等待地图区域就绪…'

    mapStore.patchRuntimeOptions({
      filterCountCardVisible: true,
      filterCountCardLoading: loading,
      filterCountCardText: countText,
    })
  }

  function syncMapRuntimeI18n() {
    mapStore.patchRuntimeOptions({
      clusterTitleTemplate: t('map.cluster.title'),
    })
    syncFilterCountCard(mapStore.isLoading)
  }

  function initializeMapDefaults() {
    mapStore.setRenderOptions(createDefaultMapRenderOptions())
    mapStore.patchRuntimeOptions(createDefaultMapRuntimeOptions(t('map.cluster.title')))
    syncFilterCountCard(mapStore.isLoading)
  }

  async function loadMapPoints() {
    const loadId = ++loadSequence
    const currentWorldId = getCurrentWorldId()
    if (!mapStore.iframeSessionReady || !currentWorldId) {
      if (loadId !== loadSequence) {
        return
      }
      mapStore.setLoading(false)
      mapPoints.value = []
      mapStore.replaceMarkers([])
      syncFilterCountCard(false)
      if (mapStore.iframeSessionReady) {
        flushMapRuntimeToIframe()
      }
      return
    }

    mapStore.setLoading(true)
    syncFilterCountCard(true)

    try {
      const backendWorldId = stripOfficialWorldVersion(currentWorldId)
      const filters = toQueryAssetsFilters(galleryStore.filter, galleryStore.includeSubfolders)
      const nextMapPoints = await queryPhotoMapPoints({
        filters,
        sortBy: galleryStore.sortBy,
        sortOrder: galleryStore.sortOrder,
        worldId: backendWorldId,
      })
      if (loadId !== loadSequence) {
        return
      }
      mapPoints.value = nextMapPoints
      syncMarkersFromMapPoints()
    } catch (error) {
      if (loadId !== loadSequence) {
        return
      }
      console.error('Failed to load map points:', error)
      mapPoints.value = []
      mapStore.replaceMarkers([])
    } finally {
      if (loadId !== loadSequence) {
        return
      }
      mapStore.setLoading(false)
      syncFilterCountCard(false)
      flushMapRuntimeToIframe()
    }
  }

  const mapQueryKey = computed(() =>
    JSON.stringify({
      filter: galleryStore.filter,
      includeSubfolders: galleryStore.includeSubfolders,
      sortBy: galleryStore.sortBy,
      sortOrder: galleryStore.sortOrder,
      iframeSessionReady: mapStore.iframeSessionReady,
      currentWorldId: getCurrentWorldId(),
    })
  )

  watch(
    mapQueryKey,
    async () => {
      await loadMapPoints()
    },
    { immediate: true }
  )

  watch(locale, () => {
    syncMapRuntimeI18n()
    if (mapStore.iframeSessionReady) {
      syncMarkersFromMapPoints()
      flushMapRuntimeToIframe()
    }
  })

  return {
    mapPoints: computed(() => mapPoints.value),
    initializeMapDefaults,
    syncMapRuntimeI18n,
    loadMapPoints,
  }
}
