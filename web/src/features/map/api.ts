import { queryPhotoMapPoints as queryInfinityNikkiPhotoMapPoints } from '@/extensions/infinity_nikki/api'
import type { QueryPhotoMapPointsParams, PhotoMapPoint } from '@/extensions/infinity_nikki/types'

export async function queryPhotoMapPoints(
  params: QueryPhotoMapPointsParams
): Promise<PhotoMapPoint[]> {
  return queryInfinityNikkiPhotoMapPoints(params)
}
