type PolygonPoint = {
  lat: number
  lng: number
}

type ExportPolygonPayload = {
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

function toValidPoints(points: ExportPolygonPayload['points']): PolygonPoint[] {
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

export function downloadPolygonJson(rawPayload: ExportPolygonPayload): boolean {
  const points = toValidPoints(rawPayload.points)
  if (points.length < 3) {
    return false
  }

  const regionName = String(rawPayload.regionName || '').trim() || 'A区域'
  const coordinateSystem = String(rawPayload.coordinateSystem || '').trim() || 'map_latlng'
  const closed = rawPayload.closed !== false
  const now = Number.isFinite(rawPayload.exportedAt) ? Number(rawPayload.exportedAt) : Date.now()
  const zMin = Number(rawPayload.zRange?.min)
  const zMax = Number(rawPayload.zRange?.max)

  const output = {
    regionName,
    coordinateSystem,
    points,
    closed,
    zRange: {
      min: Number.isFinite(zMin) ? zMin : -100000,
      max: Number.isFinite(zMax) ? zMax : 100000,
    },
    exportedAt: now,
  }

  const fileRegion = sanitizeFileNameSegment(regionName) || 'region'
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
