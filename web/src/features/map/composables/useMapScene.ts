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
import { resolveMatchedWorldId } from '@/features/map/domain/worldPolygonFilter'
import { WORLD_POLYGON_RULES } from '@/features/map/domain/worldPolygons'
import { useMapStore } from '@/features/map/store'

export function useMapScene() {
  const { t, locale } = useI18n()
  const galleryStore = useGalleryStore()
  const mapStore = useMapStore()
  const mapPoints = ref<PhotoMapPoint[]>([])

  function filterMapPointsByCurrentWorld(points: PhotoMapPoint[]): PhotoMapPoint[] {
    const currentWorldId = String(mapStore.runtimeOptions.currentWorldId ?? '').trim()
    if (!currentWorldId) {
      return points
    }

    return points.filter((point) => {
      const matchedWorldId = resolveMatchedWorldId(point, WORLD_POLYGON_RULES)
      return matchedWorldId === currentWorldId
    })
  }

  function syncMarkersFromMapPoints() {
    const filteredPoints = filterMapPointsByCurrentWorld(mapPoints.value)
    mapStore.replaceMarkers(
      toMapMarkers(filteredPoints, {
        locale: locale.value,
        thumbnailBaseUrl: mapStore.runtimeOptions.thumbnailBaseUrl,
        cardTitleFallback: t('map.popup.fallbackTitle'),
        worldId: mapStore.runtimeOptions.currentWorldId,
      })
    )
  }

  function syncFilterCountCard(loading: boolean) {
    mapStore.patchRuntimeOptions({
      filterCountCardVisible: true,
      filterCountCardLoading: loading,
      filterCountCardText: loading
        ? '正在同步照片坐标…'
        : `当前筛选下 ${mapPoints.value.length} 张照片`,
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
    mapStore.setLoading(true)
    syncFilterCountCard(true)

    try {
      const filters = toQueryAssetsFilters(galleryStore.filter, galleryStore.includeSubfolders)
      mapPoints.value = await queryPhotoMapPoints({
        filters,
        sortBy: galleryStore.sortBy,
        sortOrder: galleryStore.sortOrder,
      })
      syncMarkersFromMapPoints()
    } catch (error) {
      console.error('Failed to load map points:', error)
      mapPoints.value = []
      mapStore.replaceMarkers([])
    } finally {
      mapStore.setLoading(false)
      syncFilterCountCard(false)
    }
  }

  watch(
    () => [
      galleryStore.filter,
      galleryStore.includeSubfolders,
      galleryStore.sortBy,
      galleryStore.sortOrder,
    ],
    async () => {
      await loadMapPoints()
    },
    { deep: true, immediate: true }
  )

  watch(locale, () => {
    syncMapRuntimeI18n()
    syncMarkersFromMapPoints()
  })

  watch(
    () => mapStore.runtimeOptions.currentWorldId,
    () => {
      syncMarkersFromMapPoints()
    }
  )

  return {
    mapPoints: computed(() => mapPoints.value),
    initializeMapDefaults,
    syncMapRuntimeI18n,
    loadMapPoints,
  }
}
