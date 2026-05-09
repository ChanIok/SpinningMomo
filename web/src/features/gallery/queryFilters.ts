import type { AssetFilter, QueryAssetsFilters } from './types'

export function toQueryAssetsFilters(
  filter: AssetFilter,
  _includeSubfolders: boolean
): QueryAssetsFilters {
  return {
    folderId: filter.folderId ? Number(filter.folderId) : undefined,
    includeSubfolders: true,
    createdAtFrom: filter.createdAtFrom,
    createdAtTo: filter.createdAtTo,
    type: filter.type,
    search: filter.searchQuery,
    ratings: filter.ratings,
    reviewFlag: filter.reviewFlag,
    tagIds: filter.tagIds,
    tagMatchMode: filter.tagMatchMode,
    colorHexes: filter.colorHex ? [filter.colorHex] : undefined,
    colorDistance: filter.colorDistance,
  }
}
