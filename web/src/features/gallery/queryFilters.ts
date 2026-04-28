import type { AssetFilter, QueryAssetsFilters } from './types'

export function toQueryAssetsFilters(
  filter: AssetFilter,
  includeSubfolders: boolean
): QueryAssetsFilters {
  return {
    folderId: filter.folderId ? Number(filter.folderId) : undefined,
    includeSubfolders,
    type: filter.type,
    search: filter.searchQuery,
    rating: filter.rating,
    reviewFlag: filter.reviewFlag,
    tagIds: filter.tagIds,
    tagMatchMode: filter.tagMatchMode,
    colorHexes: filter.colorHex ? [filter.colorHex] : undefined,
    colorDistance: filter.colorDistance,
  }
}
