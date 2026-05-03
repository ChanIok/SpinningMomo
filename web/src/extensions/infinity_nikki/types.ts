import type { OperationResult, QueryAssetsFilters } from '@/features/gallery/types'
import type { SortOrder } from '@/features/gallery/types'

export interface QueryPhotoMapPointsParams {
  filters: QueryAssetsFilters
  sortBy?: string
  sortOrder?: SortOrder
  worldId: string
}

export interface PhotoMapPoint {
  assetId: number
  name: string
  hash?: string
  fileCreatedAt?: number
  nikkiLocX: number
  nikkiLocY: number
  nikkiLocZ?: number
  lat: number
  lng: number
  worldId: string
  officialWorldId: string
  assetIndex: number
}

export interface InfinityNikkiExtractedParams {
  cameraParams?: string
  timeHour?: number
  timeMin?: number
  cameraFocalLength?: number
  rotation?: number
  apertureValue?: number
  filterId?: number
  filterStrength?: number
  vignetteIntensity?: number
  lightId?: number
  lightStrength?: number
  vertical?: number
  bloomIntensity?: number
  bloomThreshold?: number
  brightness?: number
  exposure?: number
  contrast?: number
  saturation?: number
  vibrance?: number
  highlights?: number
  shadow?: number
  nikkiLocX?: number
  nikkiLocY?: number
  nikkiLocZ?: number
  nikkiHidden?: number
  poseId?: number
}

export interface InfinityNikkiUserRecord {
  dyeCode?: string
  worldId?: string
}

export interface InfinityNikkiMapArea {
  autoWorldId: string
  autoOfficialWorldId: string
  userWorldId?: string
  worldId: string
  officialWorldId: string
}

export interface InfinityNikkiDetails {
  extracted?: InfinityNikkiExtractedParams
  userRecord?: InfinityNikkiUserRecord
  mapArea?: InfinityNikkiMapArea
}

export interface InfinityNikkiMapLocalizedName {
  zh?: string
  en?: string
}

export interface InfinityNikkiMapCoordinateProfile {
  xScale: number
  xBias: number
  yScale: number
  yBias: number
}

export interface InfinityNikkiMapPolygonPoint {
  x: number
  y: number
}

export interface InfinityNikkiMapZRange {
  min?: number
  max?: number
}

export interface InfinityNikkiMapWorldRule {
  polygon: InfinityNikkiMapPolygonPoint[]
  zRange?: InfinityNikkiMapZRange
}

export interface InfinityNikkiMapWorld {
  worldId: string
  officialWorldId: string
  name: InfinityNikkiMapLocalizedName
  coordinate: InfinityNikkiMapCoordinateProfile
  rules: InfinityNikkiMapWorldRule[]
}

export interface InfinityNikkiMapConfig {
  schemaVersion: number
  defaultWorldId: string
  worlds: InfinityNikkiMapWorld[]
}

export interface GetInfinityNikkiMetadataNamesParams {
  filterId?: number
  poseId?: number
  lightId?: number
  locale?: 'zh-CN' | 'en-US'
}

export interface InfinityNikkiMetadataNames {
  filterName?: string
  poseName?: string
  lightName?: string
}

export interface SetInfinityNikkiUserRecordParams {
  assetId: number
  codeValue?: string
}

export interface PreviewInfinityNikkiSameOutfitDyeCodeFillParams {
  assetId: number
}

export interface InfinityNikkiSameOutfitDyeCodeFillPreview {
  sourceHasOutfitDyeState: boolean
  matchedCount: number
  fillableCount: number
  recordedCount: number
}

export interface FillInfinityNikkiSameOutfitDyeCodeParams {
  assetId: number
  codeValue: string
}

export interface InfinityNikkiSameOutfitDyeCodeFillResult {
  success: boolean
  message: string
  sourceHasOutfitDyeState: boolean
  matchedCount: number
  affectedCount: number
  skippedExistingCount: number
  updatedExistingCount: number
}

export interface SetInfinityNikkiWorldRecordParams {
  assetId: number
  worldId?: string
}

export type { OperationResult }
