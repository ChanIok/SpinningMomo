import type { PhotoMapPoint } from '@/features/gallery/types'
import { transformGameToMapCoordinates } from '@/features/map/domain/coordinates'
import type {
  WorldPolygonPoint,
  WorldPolygonRule,
  WorldPolygonZRange,
} from '@/features/map/domain/worldPolygons'

export function isPointInPolygon(lat: number, lng: number, polygon: WorldPolygonPoint[]): boolean {
  if (!Array.isArray(polygon) || polygon.length < 3) {
    return false
  }

  let isInside = false
  for (let i = 0, j = polygon.length - 1; i < polygon.length; j = i++) {
    const currentPoint = polygon[i]
    const previousPoint = polygon[j]
    if (!currentPoint || !previousPoint) {
      continue
    }

    const yi = currentPoint.lat
    const xi = currentPoint.lng
    const yj = previousPoint.lat
    const xj = previousPoint.lng

    const intersects =
      yi > lat !== yj > lat && lng < ((xj - xi) * (lat - yi)) / (yj - yi || Number.EPSILON) + xi
    if (intersects) {
      isInside = !isInside
    }
  }
  return isInside
}

export function matchesZRange(
  nikkiLocZ: number | undefined,
  zRange: WorldPolygonZRange | null | undefined
): boolean {
  if (!zRange) {
    return true
  }

  const hasMin = Number.isFinite(zRange.min)
  const hasMax = Number.isFinite(zRange.max)
  if (!hasMin && !hasMax) {
    return true
  }

  const z = Number(nikkiLocZ)
  if (!Number.isFinite(z)) {
    return false
  }

  if (hasMin && z < Number(zRange.min)) {
    return false
  }
  if (hasMax && z > Number(zRange.max)) {
    return false
  }
  return true
}

export function resolveMatchedWorldId(
  point: Pick<PhotoMapPoint, 'nikkiLocX' | 'nikkiLocY' | 'nikkiLocZ'>,
  orderedRules: WorldPolygonRule[]
): string | null {
  for (const rule of orderedRules) {
    const { lat, lng } = transformGameToMapCoordinates(point, rule.worldId)
    if (!isPointInPolygon(lat, lng, rule.polygon)) {
      continue
    }
    if (!matchesZRange(point.nikkiLocZ, rule.zRange)) {
      continue
    }
    return rule.worldId
  }
  return null
}
