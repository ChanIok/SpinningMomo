import type { ViewMode } from '../types'

export interface GallerySettings {
  view: {
    size: number
    mode: ViewMode
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
  }
}

export const GALLERY_SETTINGS_STORAGE_KEY = 'spinningmomo.gallery.settings'

export function createDefaultGallerySettings(): GallerySettings {
  return {
    view: {
      size: 128,
      mode: 'grid' satisfies ViewMode,
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
