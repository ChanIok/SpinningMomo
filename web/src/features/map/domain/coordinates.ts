import type { PhotoMapPoint } from '@/features/gallery/types'

// 模型A：mapX = kx * gameX + bx，mapY = ky * gameY + by
const MAP_X_SCALE = 1.00010613
const MAP_X_BIAS = 756015.585
const MAP_Y_SCALE = 1.00001142
const MAP_Y_BIAS = 392339.507

export function transformGameToMapCoordinates(
  point: Pick<PhotoMapPoint, 'nikkiLocX' | 'nikkiLocY'>
): {
  lat: number
  lng: number
} {
  const mapX = point.nikkiLocX * MAP_X_SCALE + MAP_X_BIAS
  const mapY = point.nikkiLocY * MAP_Y_SCALE + MAP_Y_BIAS

  return {
    // 官方地图经纬度约定与采集数据轴向相反，这里交换 x/y。
    lat: mapY,
    lng: mapX,
  }
}
