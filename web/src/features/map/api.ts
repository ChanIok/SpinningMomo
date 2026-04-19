import { queryPhotoMapPoints as queryPhotoMapPointsFromGallery } from '@/features/gallery/api'
import type { QueryPhotoMapPointsParams, PhotoMapPoint } from '@/features/gallery/types'

export async function queryPhotoMapPoints(
  params: QueryPhotoMapPointsParams
): Promise<PhotoMapPoint[]> {
  return queryPhotoMapPointsFromGallery(params)
}
