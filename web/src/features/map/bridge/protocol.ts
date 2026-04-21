import type { MapMarker, MapRenderOptions, MapRuntimeOptions } from '@/features/map/store'

export const MAP_URL = 'https://myl.nuanpaper.com/tools/map'
export const MAP_ORIGIN = 'https://myl.nuanpaper.com'

export const ACTION_SYNC_RUNTIME = 'SPINNING_MOMO_SYNC_RUNTIME'
/** 仅 Debug 注入脚本内为 true 时由 iframe 处理；Vite dev 用于热更 `injection/source` 整包 */
export const ACTION_EVAL_SCRIPT = 'EVAL_SCRIPT'
export const ACTION_OPEN_GALLERY_ASSET = 'SPINNING_MOMO_OPEN_GALLERY_ASSET'
export const ACTION_SET_MARKERS_VISIBLE = 'SPINNING_MOMO_SET_MARKERS_VISIBLE'
export const ACTION_EXPORT_POLYGON = 'SPINNING_MOMO_EXPORT_POLYGON'
/** iframe：地图 runtime 已挂载或 URL 中 world 已变化，payload 带当前 worldId */
export const ACTION_MAP_SESSION_READY = 'SPINNING_MOMO_MAP_SESSION_READY'

export type SyncRuntimePayload = {
  markers: MapMarker[]
  renderOptions: MapRenderOptions
  runtimeOptions: MapRuntimeOptions
}

export type SyncRuntimeMessage = {
  action: typeof ACTION_SYNC_RUNTIME
  payload: SyncRuntimePayload
}

export type OpenGalleryAssetMessage = {
  action: typeof ACTION_OPEN_GALLERY_ASSET
  payload?: {
    assetId?: number
    assetIndex?: number
  }
}

export type SetMarkersVisibleMessage = {
  action: typeof ACTION_SET_MARKERS_VISIBLE
  payload?: {
    markersVisible?: boolean
  }
}

export type ExportPolygonMessage = {
  action: typeof ACTION_EXPORT_POLYGON
  payload?: {
    regionName?: string
    coordinateSystem?: string
    points?: Array<{
      lat?: number
      lng?: number
    }>
    closed?: boolean
    zRange?: {
      min?: number
      max?: number
    }
    exportedAt?: number
  }
}

export type MapSessionReadyMessage = {
  action: typeof ACTION_MAP_SESSION_READY
  payload?: {
    worldId?: string
  }
}

export type MapInboundMessage =
  | OpenGalleryAssetMessage
  | SetMarkersVisibleMessage
  | ExportPolygonMessage
  | MapSessionReadyMessage
