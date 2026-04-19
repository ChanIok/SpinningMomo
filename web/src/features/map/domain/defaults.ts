import type { MapRenderOptions, MapRuntimeOptions } from '@/features/map/store'

export function createDefaultMapRenderOptions(): MapRenderOptions {
  return {
    mapBackgroundColor: '#C7BFA7',
    markerPinBackgroundUrl:
      'https://assets.papegames.com/nikkiweb/infinitynikki/infinitynikki-map/img/58ca045d59db0f9cd8ad.png',
    markerIconUrl:
      'https://webstatic.papegames.com/a6f47b49876cbaff/images/bg/EzoioGtm0TN1V9Ua.png',
    markerIconSize: [32, 32],
    markerIconAnchor: [14, 14],
    closePopupOnMouseOut: true,
    popupOpenDelayMs: 180,
    popupCloseDelayMs: 260,
    keepPopupVisibleOnHover: true,
  }
}

export function createDefaultMapRuntimeOptions(clusterTitleTemplate: string): MapRuntimeOptions {
  return {
    clusterEnabled: true,
    clusterRadius: 44,
    hoverCardEnabled: true,
    markersVisible: true,
    thumbnailBaseUrl: 'http://127.0.0.1:51206',
    clusterTitleTemplate,
  }
}
