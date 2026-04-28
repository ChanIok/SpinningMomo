import { call } from '@/core/rpc'
import type { OperationResult } from '@/features/gallery/types'
import type {
  FillInfinityNikkiSameOutfitDyeCodeParams,
  InfinityNikkiDetails,
  InfinityNikkiMetadataNames,
  InfinityNikkiSameOutfitDyeCodeFillPreview,
  InfinityNikkiSameOutfitDyeCodeFillResult,
  GetInfinityNikkiMetadataNamesParams,
  PhotoMapPoint,
  PreviewInfinityNikkiSameOutfitDyeCodeFillParams,
  QueryPhotoMapPointsParams,
  SetInfinityNikkiUserRecordParams,
  SetInfinityNikkiWorldRecordParams,
} from './types'

export async function queryPhotoMapPoints(
  params: QueryPhotoMapPointsParams
): Promise<PhotoMapPoint[]> {
  try {
    const result = await call<PhotoMapPoint[]>(
      'extensions.infinityNikki.queryPhotoMapPoints',
      params
    )

    console.log('🗺️ 查询地图点位成功:', {
      count: result.length,
      filters: params.filters,
    })

    return result
  } catch (error) {
    console.error('Failed to query photo map points:', error)
    throw new Error('查询地图点位失败')
  }
}

export async function getInfinityNikkiDetails(assetId: number): Promise<InfinityNikkiDetails> {
  try {
    const result = await call<InfinityNikkiDetails>('extensions.infinityNikki.getDetails', {
      assetId,
    })

    return result
  } catch (error) {
    console.error('Failed to get Infinity Nikki details:', error)
    throw new Error('获取无限暖暖详情失败')
  }
}

export async function getInfinityNikkiMetadataNames(
  params: GetInfinityNikkiMetadataNamesParams
): Promise<InfinityNikkiMetadataNames> {
  try {
    const result = await call<InfinityNikkiMetadataNames>(
      'extensions.infinityNikki.getMetadataNames',
      params
    )
    return result
  } catch (error) {
    console.error('Failed to get Infinity Nikki metadata names:', error)
    // 该接口用于“增强展示”，失败时返回空映射，让 UI 自动回退原始 ID。
    return {}
  }
}

export async function setInfinityNikkiUserRecord(
  params: SetInfinityNikkiUserRecordParams
): Promise<OperationResult> {
  try {
    const result = await call<OperationResult>('extensions.infinityNikki.setUserRecord', params)

    return result
  } catch (error) {
    console.error('Failed to set Infinity Nikki user record:', error)
    throw new Error('更新无限暖暖玩家记录失败')
  }
}

export async function previewInfinityNikkiSameOutfitDyeCodeFill(
  params: PreviewInfinityNikkiSameOutfitDyeCodeFillParams
): Promise<InfinityNikkiSameOutfitDyeCodeFillPreview> {
  try {
    const result = await call<InfinityNikkiSameOutfitDyeCodeFillPreview>(
      'extensions.infinityNikki.previewSameOutfitDyeCodeFill',
      params
    )

    return result
  } catch (error) {
    console.error('Failed to preview Infinity Nikki same outfit and dye fill:', error)
    throw new Error('获取无限暖暖相同穿搭与染色状态失败')
  }
}

export async function fillInfinityNikkiSameOutfitDyeCode(
  params: FillInfinityNikkiSameOutfitDyeCodeParams
): Promise<InfinityNikkiSameOutfitDyeCodeFillResult> {
  try {
    const result = await call<InfinityNikkiSameOutfitDyeCodeFillResult>(
      'extensions.infinityNikki.fillSameOutfitDyeCode',
      params
    )

    return result
  } catch (error) {
    console.error('Failed to fill Infinity Nikki same outfit and dye records:', error)
    throw new Error('填充无限暖暖相同穿搭与染色状态失败')
  }
}

export async function setInfinityNikkiWorldRecord(
  params: SetInfinityNikkiWorldRecordParams
): Promise<OperationResult> {
  try {
    const result = await call<OperationResult>('extensions.infinityNikki.setWorldRecord', params)

    return result
  } catch (error) {
    console.error('Failed to set Infinity Nikki world record:', error)
    throw new Error('更新无限暖暖地图区域失败')
  }
}
