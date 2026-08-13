import { computed, type Ref } from 'vue'
import { createDefaultGallerySettings, type GallerySettings } from './persistence'

interface LayoutSliceArgs {
  settings: Ref<GallerySettings>
}

/**
 * Layout Slice
 *
 * 关注点:
 * - 画廊三栏布局的真相源
 * - 左右面板开关状态与持久化宽度
 */
export function createLayoutSlice(args: LayoutSliceArgs) {
  const { settings } = args
  const sidebarOpen = computed({
    get: () => settings.value.layout.sidebarOpen,
    set: (open: boolean) => {
      settings.value.layout.sidebarOpen = open
    },
  })
  const detailsOpen = computed({
    get: () => settings.value.layout.detailsOpen,
    set: (open: boolean) => {
      settings.value.layout.detailsOpen = open
    },
  })
  const leftSidebarSize = computed({
    get: () => settings.value.layout.leftSidebarSize,
    set: (size: string) => {
      settings.value.layout.leftSidebarSize = size
    },
  })
  const rightDetailsSize = computed({
    get: () => settings.value.layout.rightDetailsSize,
    set: (size: string) => {
      settings.value.layout.rightDetailsSize = size
    },
  })
  const leftSidebarOpenSize = computed({
    get: () => settings.value.layout.leftSidebarOpenSize,
    set: (size: string) => {
      settings.value.layout.leftSidebarOpenSize = size
    },
  })
  const rightDetailsOpenSize = computed({
    get: () => settings.value.layout.rightDetailsOpenSize,
    set: (size: string) => {
      settings.value.layout.rightDetailsOpenSize = size
    },
  })
  const sidebarFolderSplitSize = computed({
    get: () => settings.value.layout.sidebarFolderSplitSize ?? 0.5,
    set: (size: number | string) => {
      settings.value.layout.sidebarFolderSplitSize = size
    },
  })

  function resetLayoutState() {
    const defaults = createDefaultGallerySettings()
    settings.value.layout = { ...defaults.layout }
  }

  return {
    sidebarOpen,
    detailsOpen,
    leftSidebarSize,
    rightDetailsSize,
    leftSidebarOpenSize,
    rightDetailsOpenSize,
    sidebarFolderSplitSize,
    resetLayoutState,
  }
}
