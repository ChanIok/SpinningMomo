import type { ViewMode } from '../types'

export type GalleryDeleteMode = 'recycleBin' | 'permanent'

export interface GallerySettings {
  view: {
    size: number
    mode: ViewMode
    showRatingBadge: boolean
    showDyeCodeBadge: boolean
    showTagBadges: boolean
  }
  navigation: {
    expandedFolderIds: number[]
    expandedTagIds: number[]
    includeSubfolders: boolean
  }
  deletion: {
    mode: GalleryDeleteMode
    confirmRecycleBin: boolean
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
      showTagBadges: true,
    },
    navigation: {
      expandedFolderIds: [],
      expandedTagIds: [],
      includeSubfolders: true,
    },
    deletion: {
      mode: 'recycleBin',
      confirmRecycleBin: false,
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

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
}

export function applySettingsDefaults<T>(stored: unknown, defaults: T): T {
  if (!isRecord(defaults)) {
    return stored === undefined ? defaults : (stored as T)
  }

  const source = isRecord(stored) ? stored : {}
  return Object.fromEntries(
    Object.entries(defaults).map(([key, defaultValue]) => [
      key,
      applySettingsDefaults(source[key], defaultValue),
    ])
  ) as T
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
