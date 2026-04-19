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
import { useMapStore } from '@/features/map/store'

export function useMapScene() {
  const { t, locale } = useI18n()
  const galleryStore = useGalleryStore()
  const mapStore = useMapStore()
  const mapPoints = ref<PhotoMapPoint[]>([])

  function syncMapRuntimeI18n() {
    mapStore.patchRuntimeOptions({
      clusterTitleTemplate: t('map.cluster.title'),
    })
  }

  function initializeMapDefaults() {
    mapStore.setRenderOptions(createDefaultMapRenderOptions())
    mapStore.patchRuntimeOptions(createDefaultMapRuntimeOptions(t('map.cluster.title')))
  }

  async function loadMapPoints() {
    mapStore.setLoading(true)

    try {
      const filters = toQueryAssetsFilters(galleryStore.filter, galleryStore.includeSubfolders)
      mapPoints.value = await queryPhotoMapPoints({
        filters,
        sortBy: galleryStore.sortBy,
        sortOrder: galleryStore.sortOrder,
      })

      mapStore.replaceMarkers(
        toMapMarkers(mapPoints.value, {
          locale: locale.value,
          thumbnailBaseUrl: mapStore.runtimeOptions.thumbnailBaseUrl,
          cardTitleFallback: t('map.popup.fallbackTitle'),
        })
      )
    } catch (error) {
      console.error('Failed to load map points:', error)
      mapPoints.value = []
      mapStore.replaceMarkers([])
    } finally {
      mapStore.setLoading(false)
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
    void loadMapPoints()
  })

  return {
    mapPoints: computed(() => mapPoints.value),
    initializeMapDefaults,
    syncMapRuntimeI18n,
    loadMapPoints,
  }
}
