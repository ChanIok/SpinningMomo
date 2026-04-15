import { defineStore } from 'pinia'
import { ref } from 'vue'

export interface MapMarker {
  assetId?: number
  name?: string
  lat: number
  lng: number
  popupHtml: string
  thumbnailUrl?: string
  fileCreatedAt?: number
}

export interface MapRenderOptions {
  /** 游戏地图标点外框底图（PNG） */
  markerPinBackgroundUrl?: string
  markerIconUrl?: string
  markerIconSize?: [number, number]
  markerIconAnchor?: [number, number]
  popupAnchor?: [number, number]
  openPopupOnHover?: boolean
  closePopupOnMouseOut?: boolean
  popupOpenDelayMs?: number
  popupCloseDelayMs?: number
  keepPopupVisibleOnHover?: boolean
}

export interface MapRuntimeOptions {
  clusterEnabled: boolean
  clusterRadius: number
  hoverCardEnabled: boolean
  thumbnailBaseUrl: string
  /** 聚合 hover 标题模板，含 {count}，由 i18n 注入 */
  clusterTitleTemplate?: string
}

export const useMapStore = defineStore('map', () => {
  const markers = ref<MapMarker[]>([])
  const isLoading = ref(false)
  const renderOptions = ref<MapRenderOptions>({})
  const runtimeOptions = ref<MapRuntimeOptions>({
    clusterEnabled: true,
    clusterRadius: 44,
    hoverCardEnabled: true,
    thumbnailBaseUrl: 'http://127.0.0.1:51206',
  })

  const setMarkers = (nextMarkers: MapMarker[]) => {
    markers.value = nextMarkers
  }

  const setRenderOptions = (nextOptions: MapRenderOptions) => {
    renderOptions.value = { ...nextOptions }
  }

  const setRuntimeOptions = (nextOptions: Partial<MapRuntimeOptions>) => {
    runtimeOptions.value = {
      ...runtimeOptions.value,
      ...nextOptions,
    }
  }

  return {
    markers,
    isLoading,
    renderOptions,
    runtimeOptions,
    setMarkers,
    setRenderOptions,
    setRuntimeOptions,
  }
})
