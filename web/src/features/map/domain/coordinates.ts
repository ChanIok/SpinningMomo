import type { PhotoMapPoint } from '@/features/gallery/types'

type CoordinateTransformProfile = {
  xScale: number
  xBias: number
  yScale: number
  yBias: number
}

// 默认参数对应 worldId=1.1
const DEFAULT_WORLD_ID = '1.1'
const COORDINATE_PROFILES: Record<string, CoordinateTransformProfile> = {
  '1.1': {
    xScale: 1,
    xBias: 756005.593,
    yScale: 1,
    yBias: 392338.66,
  },
  '10000001.1': {
    xScale: 1,
    xBias: 316683.761,
    yScale: 1,
    yBias: -535333.456,
  },
  '10000002.1': {
    xScale: 1,
    xBias: 104945.06811,
    yScale: 1,
    yBias: 19018.608232,
  },
  '10000010.1': {
    xScale: 1,
    xBias: 402475.09316,
    yScale: 1,
    yBias: -174121.062976,
  },
  '10000027.1': {
    xScale: 1,
    xBias: 412200.62985,
    yScale: 1,
    yBias: -145027.655483,
  },
  '4020034.3': {
    xScale: 1,
    xBias: 87752.06396,
    yScale: 1,
    yBias: 44300.600838,
  },
}

function resolveCoordinateProfile(worldId?: string): CoordinateTransformProfile {
  const normalizedWorldId = String(worldId ?? '').trim()
  const matchedProfile = normalizedWorldId ? COORDINATE_PROFILES[normalizedWorldId] : undefined
  if (matchedProfile) {
    return matchedProfile
  }
  return COORDINATE_PROFILES[DEFAULT_WORLD_ID]!
}

export function transformGameToMapCoordinates(
  point: Pick<PhotoMapPoint, 'nikkiLocX' | 'nikkiLocY'>,
  worldId?: string
): {
  lat: number
  lng: number
} {
  const profile = resolveCoordinateProfile(worldId)
  const mapX = point.nikkiLocX * profile.xScale + profile.xBias
  const mapY = point.nikkiLocY * profile.yScale + profile.yBias

  return {
    // 官方地图经纬度约定与采集数据轴向相反，这里交换 x/y。
    lat: mapY,
    lng: mapX,
  }
}

export function transformMapToGameCoordinates(
  point: {
    lat: number
    lng: number
  },
  worldId?: string
): {
  x: number
  y: number
} {
  const profile = resolveCoordinateProfile(worldId)

  return {
    x: (point.lng - profile.xBias) / profile.xScale,
    y: (point.lat - profile.yBias) / profile.yScale,
  }
}
