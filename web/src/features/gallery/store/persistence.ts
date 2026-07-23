import type { ViewMode } from '../types'

export interface GallerySettings {
  view: {
    size: number
    mode: ViewMode
    showRatingBadge: boolean
    showDyeCodeBadge: boolean
  }
  navigation: {
    expandedFolderIds: number[]
    expandedTagIds: number[]
  }
  layout: {
    sidebarOpen: boolean
    detailsOpen: boolean
    leftSidebarSize: string
    rightDetailsSize: string
    leftSidebarOpenSize: string
    rightDetailsOpenSize: string
    sidebarFolderSplitSize: number | string
  }
}

export const GALLERY_SETTINGS_STORAGE_KEY = 'spinningmomo.gallery.settings'

export function createDefaultGallerySettings(): GallerySettings {
  return {
    view: {
      size: 256,
      mode: 'grid' satisfies ViewMode,
      showRatingBadge: true,
      showDyeCodeBadge: true,
    },
    navigation: {
      expandedFolderIds: [],
      expandedTagIds: [],
    },
    layout: {
      sidebarOpen: true,
      detailsOpen: true,
      leftSidebarSize: '200px',
      rightDetailsSize: '256px',
      leftSidebarOpenSize: '200px',
      rightDetailsOpenSize: '256px',
      sidebarFolderSplitSize: 0.5,
    },
  }
}

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
