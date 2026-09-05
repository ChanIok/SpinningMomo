import { computed, ref } from 'vue'
import { galleryApi } from '../api'
import { useGalleryStore } from '../store'
import { toQueryAssetsFilters } from '../queryFilters'
import type { AssetLayoutMetaItem } from '../types'

/**
 * 统一拉取“布局元数据”（仅 id/宽高等轻量信息），
 * 供 adaptive 与 masonry 两种虚拟布局复用，并在 Store 中缓存，避免重复实现相同查询逻辑。
 */
export function useGalleryLayoutMeta(logScope?: string) {
  const store = useGalleryStore()
  // 递增请求序号：用于丢弃过期响应，避免快速切换筛选/排序时旧数据回写。
  const layoutRequestId = ref(0)
  const isFetching = ref(false)

  async function reloadLayoutMeta(): Promise<AssetLayoutMetaItem[]> {
    const requestId = layoutRequestId.value + 1
    layoutRequestId.value = requestId
    isFetching.value = true

    // 保持与 gallery.queryAssets 一致的过滤语义，确保布局与真实数据顺序对齐。
    const filters = toQueryAssetsFilters(store.filter, store.includeSubfolders)

    try {
      const response = await galleryApi.queryAssetLayoutMeta({
        filters,
        sortBy: store.sortBy,
        sortOrder: store.sortOrder,
      })

      if (layoutRequestId.value !== requestId) {
        return []
      }

      // 仅在“当前仍是最新请求”时更新，保证 layoutMetaItems 始终对应当前查询条件。
      store.setLayoutMetaItems(response.items)
      return response.items
    } catch (error) {
      if (layoutRequestId.value !== requestId) {
        return []
      }

      // 失败时清空，避免复用陈旧布局导致可视区索引与数据页错位。
      store.clearLayoutMetaItems()
      if (logScope) {
        console.error(`Failed to reload ${logScope} layout meta:`, error)
      }
      return []
    } finally {
      if (layoutRequestId.value === requestId) {
        isFetching.value = false
      }
    }
  }

  async function ensureLayoutMetaLoaded(): Promise<AssetLayoutMetaItem[]> {
    // 若总数为 0，或已缓存的布局元数据条数与当前总数一致，直接复用缓存
    if (
      store.totalCount === 0 ||
      (store.layoutMetaItems.length === store.totalCount && store.totalCount > 0)
    ) {
      return store.layoutMetaItems
    }
    return reloadLayoutMeta()
  }

  return {
    layoutMetaItems: computed(() => store.layoutMetaItems),
    reloadLayoutMeta,
    ensureLayoutMetaLoaded,
    isFetching,
  }
}
