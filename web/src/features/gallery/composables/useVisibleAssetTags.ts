import { onScopeDispose, watch } from 'vue'
import { galleryApi } from '../api'
import { useGalleryStore } from '../store'

const VISIBLE_TAGS_DEBOUNCE_MS = 100
const VISIBLE_TAGS_BATCH_SIZE = 400

function chunkAssetIds(assetIds: number[]): number[][] {
  const chunks: number[][] = []
  for (let start = 0; start < assetIds.length; start += VISIBLE_TAGS_BATCH_SIZE) {
    chunks.push(assetIds.slice(start, start + VISIBLE_TAGS_BATCH_SIZE))
  }
  return chunks
}

/**
 * 只为虚拟列表当前存活范围加载标签。查询结果跨筛选复用，
 * 无标签资产也会被明确缓存，避免滚动时重复请求。
 */
export function useVisibleAssetTags() {
  const store = useGalleryStore()
  const inFlightAssetIds = new Set<number>()
  let debounceTimer: ReturnType<typeof setTimeout> | undefined
  let disposed = false

  function scheduleVisibleTagLoad() {
    if (debounceTimer !== undefined) {
      clearTimeout(debounceTimer)
    }

    debounceTimer = setTimeout(() => {
      debounceTimer = undefined
      void loadVisibleAssetTags()
    }, VISIBLE_TAGS_DEBOUNCE_MS)
  }

  async function loadVisibleAssetTags() {
    if (disposed || !store.gallerySettings.view.showTagBadges) {
      return
    }

    const { startIndex, endIndex } = store.visibleRange
    if (startIndex === undefined || endIndex === undefined) {
      return
    }

    const pendingAssetIds = [
      ...new Set(
        store
          .getAssetsInRange(startIndex, endIndex)
          .flatMap((asset) => (asset === null ? [] : [asset.id]))
      ),
    ].filter((assetId) => !store.loadedAssetTagIds.has(assetId) && !inFlightAssetIds.has(assetId))

    for (const batch of chunkAssetIds(pendingAssetIds)) {
      const requestEpoch = store.assetTagsEpoch
      batch.forEach((assetId) => inFlightAssetIds.add(assetId))

      try {
        const tagsByAssetId = await galleryApi.getTagsByAssetIds(batch)
        if (!disposed && requestEpoch === store.assetTagsEpoch) {
          store.setAssetTagsForAssets(batch, tagsByAssetId)
        }
      } catch (error) {
        // 标签胶囊是增强展示，请求失败不阻断图库主体。
        console.warn('Failed to load visible gallery tags:', error)
      } finally {
        batch.forEach((assetId) => inFlightAssetIds.delete(assetId))
        if (!disposed && requestEpoch !== store.assetTagsEpoch) {
          scheduleVisibleTagLoad()
        }
      }
    }
  }

  watch(
    () => [
      store.gallerySettings.view.showTagBadges,
      store.visibleRange.startIndex,
      store.visibleRange.endIndex,
      store.paginatedAssetsVersion,
      store.assetTagsEpoch,
    ],
    scheduleVisibleTagLoad,
    { immediate: true }
  )

  onScopeDispose(() => {
    disposed = true
    if (debounceTimer !== undefined) {
      clearTimeout(debounceTimer)
    }
  })
}
