import { normalizeOfficialWorldId } from '@/features/map/domain/officialWorldId'
import { transformMapToGameCoordinates } from '@/features/map/domain/coordinates'

type RawMapPoint = {
  lat: number
  lng: number
}

type PolygonPoint = {
  x: number
  y: number
}

type ZRange = {
  min: number
  max: number
}

type ExportedPolygonJson = {
  regionName: string
  worldId: string
  coordinateSystem: 'game_xy'
  points: PolygonPoint[]
  closed: boolean
  zRange: ZRange
  exportedAt: number
}

type ExportPolygonPayload = {
  worldId?: string
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

function sanitizeFileNameSegment(value: string): string {
  return value.replace(/[<>:"/\\|?*\u0000-\u001f]/g, '_').trim()
}

function toValidMapPoints(points: ExportPolygonPayload['points']): RawMapPoint[] {
  if (!Array.isArray(points)) {
    return []
  }

  return points
    .map((point) => ({
      lat: Number(point?.lat),
      lng: Number(point?.lng),
    }))
    .filter((point) => Number.isFinite(point.lat) && Number.isFinite(point.lng))
}

function toGamePolygonPoints(points: RawMapPoint[], worldId: string): PolygonPoint[] {
  return points.map((point) => transformMapToGameCoordinates(point, worldId))
}

function toValidZRange(rawPayload: ExportPolygonPayload): ZRange {
  const zMin = Number(rawPayload.zRange?.min)
  const zMax = Number(rawPayload.zRange?.max)

  return {
    min: Number.isFinite(zMin) ? zMin : -100000,
    max: Number.isFinite(zMax) ? zMax : 100000,
  }
}

function toExportedPolygonJson(
  rawPayload: ExportPolygonPayload,
  worldId: string,
  points: PolygonPoint[],
  exportedAt: number
): ExportedPolygonJson {
  return {
    regionName: String(rawPayload.regionName || '').trim() || 'A区域',
    worldId,
    coordinateSystem: 'game_xy',
    points,
    closed: rawPayload.closed !== false,
    zRange: toValidZRange(rawPayload),
    exportedAt,
  }
}

export function downloadPolygonJson(rawPayload: ExportPolygonPayload): boolean {
  const worldId = normalizeOfficialWorldId(rawPayload.worldId)
  const mapPoints = toValidMapPoints(rawPayload.points)
  if (!worldId || mapPoints.length < 3) {
    return false
  }

  const now = Number.isFinite(rawPayload.exportedAt) ? Number(rawPayload.exportedAt) : Date.now()
  const output = toExportedPolygonJson(
    rawPayload,
    worldId,
    toGamePolygonPoints(mapPoints, worldId),
    now
  )
  const fileRegion = sanitizeFileNameSegment(output.regionName) || 'region'
  const fileName = `polygon_${fileRegion}_${new Date(now).toISOString().replace(/[:.]/g, '-')}.json`
  const blob = new Blob([JSON.stringify(output, null, 2)], {
    type: 'application/json;charset=utf-8',
  })
  const objectUrl = URL.createObjectURL(blob)

  const anchor = document.createElement('a')
  anchor.href = objectUrl
  anchor.download = fileName
  anchor.rel = 'noopener'
  document.body.appendChild(anchor)
  anchor.click()
  anchor.remove()

  window.setTimeout(() => {
    URL.revokeObjectURL(objectUrl)
  }, 0)

  return true
}
