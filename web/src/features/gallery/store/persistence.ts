export const GALLERY_VIEW_SIZE_STORAGE_KEY = 'spinningmomo.gallery.view.size'
export const GALLERY_VIEW_MODE_STORAGE_KEY = 'spinningmomo.gallery.view.mode'
// 侧边栏树的展开状态是纯前端视图信息，只需落在 localStorage。
export const GALLERY_EXPANDED_FOLDERS_STORAGE_KEY = 'spinningmomo.gallery.sidebar.expanded-folders'
export const GALLERY_EXPANDED_TAGS_STORAGE_KEY = 'spinningmomo.gallery.sidebar.expanded-tags'

export const LIGHTBOX_MIN_ZOOM = 0.05
export const LIGHTBOX_MAX_ZOOM = 5

export function collectTreeIds<T extends { id: number; children: T[] }>(nodes: T[]): number[] {
  const ids: number[] = []

  for (const node of nodes) {
    ids.push(node.id)
    ids.push(...collectTreeIds(node.children))
  }

  return ids
}
