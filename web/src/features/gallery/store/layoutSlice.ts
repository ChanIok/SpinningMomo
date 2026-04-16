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
  const sidebarOpen = computed(() => settings.value.layout.sidebarOpen)
  const detailsOpen = computed(() => settings.value.layout.detailsOpen)
  const leftSidebarSize = computed(() => settings.value.layout.leftSidebarSize)
  const rightDetailsSize = computed(() => settings.value.layout.rightDetailsSize)
  const leftSidebarOpenSize = computed(() => settings.value.layout.leftSidebarOpenSize)
  const rightDetailsOpenSize = computed(() => settings.value.layout.rightDetailsOpenSize)

  function setSidebarOpen(open: boolean) {
    settings.value.layout.sidebarOpen = open
  }

  function setDetailsOpen(open: boolean) {
    settings.value.layout.detailsOpen = open
  }

  function setLeftSidebarSize(size: string) {
    settings.value.layout.leftSidebarSize = size
  }

  function setRightDetailsSize(size: string) {
    settings.value.layout.rightDetailsSize = size
  }

  function setLeftSidebarOpenSize(size: string) {
    settings.value.layout.leftSidebarOpenSize = size
  }

  function setRightDetailsOpenSize(size: string) {
    settings.value.layout.rightDetailsOpenSize = size
  }

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
    setSidebarOpen,
    setDetailsOpen,
    setLeftSidebarSize,
    setRightDetailsSize,
    setLeftSidebarOpenSize,
    setRightDetailsOpenSize,
    resetLayoutState,
  }
}
