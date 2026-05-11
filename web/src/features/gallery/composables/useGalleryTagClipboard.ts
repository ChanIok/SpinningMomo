import { computed, reactive, readonly } from 'vue'
import type { Tag } from '../types'

interface GalleryTagClipboardState {
  sourceAssetId?: number
  tagIds: number[]
  tagNames: string[]
}

// 会话级标签剪贴板：只服务当前图库页面，不落到系统剪贴板或本地持久化。
const state = reactive<GalleryTagClipboardState>({
  sourceAssetId: undefined,
  tagIds: [],
  tagNames: [],
})

export function useGalleryTagClipboard() {
  const hasPayload = computed(() => state.tagIds.length > 0)

  function setFromTags(sourceAssetId: number, tags: Pick<Tag, 'id' | 'name'>[]) {
    const seenTagIds = new Set<number>()
    // 复制标签时保持原顺序，同时裁掉无效/重复标签，避免后续粘贴重复请求。
    const normalizedTags = tags.filter((tag) => {
      if (tag.id <= 0 || seenTagIds.has(tag.id)) {
        return false
      }

      seenTagIds.add(tag.id)
      return true
    })

    state.sourceAssetId = sourceAssetId
    state.tagIds = normalizedTags.map((tag) => tag.id)
    state.tagNames = normalizedTags.map((tag) => tag.name)
  }

  function clear() {
    state.sourceAssetId = undefined
    state.tagIds = []
    state.tagNames = []
  }

  return {
    state: readonly(state),
    hasPayload,
    setFromTags,
    clear,
  }
}
