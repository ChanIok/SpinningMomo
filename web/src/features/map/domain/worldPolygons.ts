export type WorldPolygonPoint = {
  lat: number
  lng: number
}

export type WorldPolygonZRange = {
  min?: number
  max?: number
}

export type WorldPolygonRule = {
  worldId: string
  polygon: WorldPolygonPoint[]
  /** null/undefined 表示不限制 z */
  zRange?: WorldPolygonZRange | null
}

/**
 * world 命中优先级按数组顺序判定：先匹配先命中。
 */
export const WORLD_POLYGON_RULES: WorldPolygonRule[] = [
  {
    worldId: '10000001.1',
    polygon: [
      { lat: 10000, lng: 60000 },
      { lat: 4000, lng: 85000 },
      { lat: 10000, lng: 110000 },
      { lat: 40000, lng: 135000 },
      { lat: 65000, lng: 120000 },
      { lat: 75000, lng: 80000 },
      { lat: 70000, lng: 50000 },
      { lat: 40000, lng: 40000 },
    ],
    zRange: {
      min: 0,
      max: 55000,
    },
  },
  {
    worldId: '10000002.1',
    polygon: [
      { lat: 28000, lng: 48000 },
      { lat: 16000, lng: 90000 },
      { lat: 28000, lng: 112000 },
      { lat: 60000, lng: 125000 },
      { lat: 92000, lng: 90000 },
      { lat: 92000, lng: 48000 },
      { lat: 56000, lng: 42000 },
    ],
    zRange: {
      min: 0,
      max: 32000,
    },
  },
  {
    worldId: '10000010.1',
    polygon: [
      { lat: 12000, lng: 78000 },
      { lat: 10000, lng: 120000 },
      { lat: 90000, lng: 190000 },
      { lat: 150000, lng: 170000 },
      { lat: 160000, lng: 140000 },
      { lat: 140000, lng: 80000 },
      { lat: 40000, lng: 70000 },
    ],
    zRange: {
      min: -20000,
      max: 5000,
    },
  },
  {
    worldId: '10000027.1',
    polygon: [
      { lat: 4000, lng: 52000 },
      { lat: 5000, lng: 86000 },
      { lat: 80000, lng: 170000 },
      { lat: 125000, lng: 196000 },
      { lat: 165000, lng: 162000 },
      { lat: 160000, lng: 100000 },
      { lat: 60000, lng: 40000 },
    ],
    zRange: {
      min: -20000,
      max: 5000,
    },
  },
  {
    worldId: '4020034.3',
    polygon: [
      { lat: 44000, lng: 130000 },
      { lat: 44000, lng: 170000 },
      { lat: 100000, lng: 230000 },
      { lat: 140000, lng: 240000 },
      { lat: 160000, lng: 230000 },
      { lat: 165000, lng: 180000 },
      { lat: 150000, lng: 110000 },
      { lat: 120000, lng: 88000 },
    ],
    zRange: {
      min: -16000,
      max: 60000,
    },
  },
]
