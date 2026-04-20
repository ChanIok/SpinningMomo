import { defineStore } from 'pinia'
import { ref } from 'vue'

export interface MapMarker {
  assetId?: number
  /** 与当前图库查询排序一致的资产序号，用于从地图打开时定位 lightbox */
  assetIndex?: number
  name?: string
  lat: number
  lng: number
  /** 悬停卡片标题（宿主侧已按语言格式化） */
  cardTitle: string
  thumbnailUrl?: string
  fileCreatedAt?: number
}

export interface MapRenderOptions {
  /** Leaflet 地图容器底色，用于填充无瓦片区域 */
  mapBackgroundColor?: string
  /** 游戏地图标点外框底图（PNG） */
  markerPinBackgroundUrl?: string
  markerIconUrl?: string
  markerIconSize?: [number, number]
  markerIconAnchor?: [number, number]
  closePopupOnMouseOut?: boolean
  popupOpenDelayMs?: number
  popupCloseDelayMs?: number
  keepPopupVisibleOnHover?: boolean
}

export interface MapRuntimeOptions {
  clusterEnabled: boolean
  clusterRadius: number
  hoverCardEnabled: boolean
  markersVisible: boolean
  currentWorldId?: string
  thumbnailBaseUrl: string
  /** 聚合 hover 标题模板，含 {count}，由 i18n 注入 */
  clusterTitleTemplate?: string
  filterCountCardVisible?: boolean
  filterCountCardLoading?: boolean
  filterCountCardText?: string
  filterCountCardBgColor?: string
  filterCountCardTextColor?: string
}

export const useMapStore = defineStore('map', () => {
  const markers = ref<MapMarker[]>([])
  const isLoading = ref(false)
  const renderOptions = ref<MapRenderOptions>({})
  const runtimeOptions = ref<MapRuntimeOptions>({
    clusterEnabled: true,
    clusterRadius: 44,
    hoverCardEnabled: true,
    markersVisible: true,
    thumbnailBaseUrl: 'http://127.0.0.1:51206',
  })

  const replaceMarkers = (nextMarkers: MapMarker[]) => {
    markers.value = nextMarkers
  }

  const setRenderOptions = (nextOptions: MapRenderOptions) => {
    renderOptions.value = { ...nextOptions }
  }

  const patchRuntimeOptions = (nextOptions: Partial<MapRuntimeOptions>) => {
    runtimeOptions.value = {
      ...runtimeOptions.value,
      ...nextOptions,
    }
  }

  const setLoading = (nextLoading: boolean) => {
    isLoading.value = nextLoading
  }

  return {
    markers,
    isLoading,
    renderOptions,
    runtimeOptions,
    replaceMarkers,
    setRenderOptions,
    patchRuntimeOptions,
    setLoading,
  }
})
