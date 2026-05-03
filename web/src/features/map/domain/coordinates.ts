import type {
  InfinityNikkiMapConfig,
  InfinityNikkiMapCoordinateProfile,
} from '@/extensions/infinity_nikki/types'
import { normalizeInfinityNikkiWorldId } from '@/extensions/infinity_nikki/worlds'

function resolveCoordinateProfile(
  worldId: unknown,
  config: InfinityNikkiMapConfig
): InfinityNikkiMapCoordinateProfile | undefined {
  const normalizedWorldId = normalizeInfinityNikkiWorldId(worldId)
  if (!normalizedWorldId) {
    return undefined
  }
  return config.worlds.find((world) => world.worldId === normalizedWorldId)?.coordinate
}

export function transformMapToGameCoordinates(
  point: {
    lat: number
    lng: number
  },
  worldId: unknown,
  config: InfinityNikkiMapConfig
): {
  x: number
  y: number
} | null {
  const profile = resolveCoordinateProfile(worldId, config)
  if (!profile) {
    return null
  }

  return {
    x: (point.lng - profile.xBias) / profile.xScale,
    y: (point.lat - profile.yBias) / profile.yScale,
  }
}
