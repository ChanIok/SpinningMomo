import type { Ref } from 'vue'
import type { Router } from 'vue-router'
import { useGalleryStore } from '@/features/gallery/store'
import {
  ACTION_EVAL_SCRIPT,
  ACTION_EXPORT_POLYGON,
  ACTION_MAP_SESSION_READY,
  ACTION_OPEN_GALLERY_ASSET,
  ACTION_SET_MARKERS_VISIBLE,
  ACTION_SYNC_RUNTIME,
  MAP_ORIGIN,
  type MapInboundMessage,
  type SyncRuntimeMessage,
  type SyncRuntimePayload,
} from '@/features/map/bridge/protocol'
import {
  applyIframeSessionReadyThenFlush,
  flushMapRuntimeToIframe,
} from '@/features/map/composables/mapIframeRuntime'
import { normalizeOfficialWorldId } from '@/features/map/domain/officialWorldId'
import { downloadPolygonJson } from '@/features/map/domain/polygonExport'
import { buildMapDevEvalScript } from '@/features/map/injection/mapDevEvalScript'
import { useMapStore } from '@/features/map/store'

type UseMapBridgeOptions = {
  mapIframe: Ref<HTMLIFrameElement | null>
  mapStore: ReturnType<typeof useMapStore>
  galleryStore: ReturnType<typeof useGalleryStore>
  router: Router
}

function isAllowedMapMessageOrigin(origin: string): boolean {
  if (origin === MAP_ORIGIN) {
    return true
  }
  if (import.meta.env.DEV && typeof window !== 'undefined' && origin === window.location.origin) {
    return true
  }
  return false
}

function buildSerializableRuntimePayload(
  mapStore: ReturnType<typeof useMapStore>
): SyncRuntimePayload {
  const markerIconSize = mapStore.renderOptions.markerIconSize
    ? ([...mapStore.renderOptions.markerIconSize] as [number, number])
    : undefined
  const markerIconAnchor = mapStore.renderOptions.markerIconAnchor
    ? ([...mapStore.renderOptions.markerIconAnchor] as [number, number])
    : undefined

  return {
    markers: mapStore.markers.map((marker) => ({
      assetId: marker.assetId,
      assetIndex: marker.assetIndex,
      name: marker.name,
      lat: marker.lat,
      lng: marker.lng,
      cardTitle: marker.cardTitle,
      thumbnailUrl: marker.thumbnailUrl,
      fileCreatedAt: marker.fileCreatedAt,
    })),
    renderOptions: {
      mapBackgroundColor: mapStore.renderOptions.mapBackgroundColor,
      markerPinBackgroundUrl: mapStore.renderOptions.markerPinBackgroundUrl,
      markerIconUrl: mapStore.renderOptions.markerIconUrl,
      markerIconSize,
      markerIconAnchor,
      closePopupOnMouseOut: mapStore.renderOptions.closePopupOnMouseOut,
      popupOpenDelayMs: mapStore.renderOptions.popupOpenDelayMs,
      popupCloseDelayMs: mapStore.renderOptions.popupCloseDelayMs,
      keepPopupVisibleOnHover: mapStore.renderOptions.keepPopupVisibleOnHover,
    },
    runtimeOptions: {
      clusterEnabled: mapStore.runtimeOptions.clusterEnabled,
      clusterRadius: mapStore.runtimeOptions.clusterRadius,
      hoverCardEnabled: mapStore.runtimeOptions.hoverCardEnabled,
      markersVisible: mapStore.runtimeOptions.markersVisible,
      currentWorldId: mapStore.runtimeOptions.currentWorldId,
      thumbnailBaseUrl: mapStore.runtimeOptions.thumbnailBaseUrl,
      clusterTitleTemplate: mapStore.runtimeOptions.clusterTitleTemplate,
      filterCountCardVisible: mapStore.runtimeOptions.filterCountCardVisible,
      filterCountCardLoading: mapStore.runtimeOptions.filterCountCardLoading,
      filterCountCardText: mapStore.runtimeOptions.filterCountCardText,
      filterCountCardBgColor: mapStore.runtimeOptions.filterCountCardBgColor,
      filterCountCardTextColor: mapStore.runtimeOptions.filterCountCardTextColor,
    },
  }
}

export function useMapBridge(options: UseMapBridgeOptions) {
  const { mapIframe, mapStore, galleryStore, router } = options

  function postRuntimeSync() {
    const contentWindow = mapIframe.value?.contentWindow
    if (!contentWindow) {
      return
    }

    const payload = buildSerializableRuntimePayload(mapStore)
    const targetOrigin = import.meta.env.DEV ? '*' : MAP_ORIGIN

    if (import.meta.env.DEV) {
      const script = buildMapDevEvalScript(payload)
      try {
        contentWindow.postMessage(
          {
            action: ACTION_EVAL_SCRIPT,
            payload: { script },
          },
          targetOrigin
        )
      } catch (error) {
        console.error('[MapBridge] Failed to post dev eval script:', error)
      }
      return
    }

    const message: SyncRuntimeMessage = {
      action: ACTION_SYNC_RUNTIME,
      payload,
    }
    try {
      contentWindow.postMessage(message, targetOrigin)
    } catch (error) {
      console.error('[MapBridge] Failed to post runtime payload:', error)
    }
  }

  async function handleMapMessage(event: MessageEvent<unknown>) {
    if (!isAllowedMapMessageOrigin(event.origin)) {
      return
    }

    const data = event.data as MapInboundMessage
    if (!data?.action) {
      return
    }

    if (data.action === ACTION_OPEN_GALLERY_ASSET) {
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
      return
    }

    if (data.action === ACTION_SET_MARKERS_VISIBLE) {
      const markersVisible = data.payload?.markersVisible
      if (typeof markersVisible !== 'boolean') {
        return
      }
      mapStore.patchRuntimeOptions({ markersVisible })
      flushMapRuntimeToIframe()
      return
    }

    if (data.action === ACTION_EXPORT_POLYGON) {
      const success = downloadPolygonJson(data.payload ?? {})
      if (!success) {
        console.warn(
          '[MapBridge] Polygon export ignored: at least 3 valid lat/lng points are required.'
        )
      }
      return
    }

    if (data.action === ACTION_MAP_SESSION_READY) {
      const worldId = normalizeOfficialWorldId(data.payload?.worldId)
      mapStore.patchRuntimeOptions({
        currentWorldId: worldId,
      })
      mapStore.markIframeSessionReady()
      applyIframeSessionReadyThenFlush()
      return
    }
  }

  return {
    postRuntimeSync,
    handleMapMessage,
  }
}
