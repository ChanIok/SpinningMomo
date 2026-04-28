import type { Locale } from '@/core/i18n/types'
import type { PhotoMapPoint } from '@/extensions/infinity_nikki/types'
import { transformGameToMapCoordinates } from '@/features/map/domain/coordinates'
import type { MapMarker } from '@/features/map/store'

type MarkerMapperContext = {
  locale: Locale
  thumbnailBaseUrl: string
  cardTitleFallback: string
  worldId?: string
}

const FILENAME_DATE_PREFIX_RE = /^(\d{4})_(\d{2})_(\d{2})_(\d{2})_(\d{2})_(\d{2})/

function formatPopupTitleFromFilename(
  fileName: string,
  currentLocale: Locale,
  fallbackTitle: string
): string {
  const base = fileName.replace(/^.*[/\\]/, '')
  const matched = base.match(FILENAME_DATE_PREFIX_RE)
  if (!matched) {
    return fallbackTitle
  }

  const year = Number(matched[1])
  const month = Number(matched[2])
  const day = Number(matched[3])
  if (!Number.isFinite(year) || !Number.isFinite(month) || !Number.isFinite(day)) {
    return fallbackTitle
  }

  if (currentLocale === 'zh-CN') {
    return `${year}年${month}月${day}日`
  }

  try {
    return new Date(year, month - 1, day).toLocaleDateString('en-US', {
      year: 'numeric',
      month: 'long',
      day: 'numeric',
    })
  } catch {
    return fallbackTitle
  }
}

function buildThumbnailUrl(point: PhotoMapPoint, thumbnailBaseUrl: string): string {
  if (!point.hash) {
    return ''
  }

  const prefix1 = point.hash.slice(0, 2)
  const prefix2 = point.hash.slice(2, 4)
  const relativePath = `${prefix1}/${prefix2}/${point.hash}.webp`
  const normalizedBaseUrl = thumbnailBaseUrl.replace(/\/+$/, '')
  return `${normalizedBaseUrl}/static/assets/thumbnails/${relativePath}`
}

export function toMapMarkers(points: PhotoMapPoint[], context: MarkerMapperContext): MapMarker[] {
  return points.map((point) => {
    const { lat, lng } = transformGameToMapCoordinates(point, context.worldId)
    const thumbnailUrl = buildThumbnailUrl(point, context.thumbnailBaseUrl)

    return {
      assetId: point.assetId,
      assetIndex: point.assetIndex,
      name: point.name,
      lat,
      lng,
      thumbnailUrl,
      fileCreatedAt: point.fileCreatedAt,
      cardTitle: formatPopupTitleFromFilename(
        point.name,
        context.locale,
        context.cardTitleFallback
      ),
    }
  })
}
