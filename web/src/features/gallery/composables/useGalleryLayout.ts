import { computed } from 'vue'
import { useGalleryStore } from '../store'

/**
 * Gallery 布局管理 Composable
 * 管理侧边栏和详情面板的显示状态
 */
export function useGalleryLayout() {
  const store = useGalleryStore()

  // ============= 布局状态 =============
  const isSidebarOpen = computed(() => store.sidebarOpen)
  const isDetailsOpen = computed(() => store.detailsOpen)
  const leftSidebarSize = computed({
    get: () => store.leftSidebarSize,
    set: (size: string) => store.setLeftSidebarSize(size),
  })
  const rightDetailsSize = computed({
    get: () => store.rightDetailsSize,
    set: (size: string) => store.setRightDetailsSize(size),
  })
  const leftSidebarOpenSize = computed({
    get: () => store.leftSidebarOpenSize,
    set: (size: string) => store.setLeftSidebarOpenSize(size),
  })
  const rightDetailsOpenSize = computed({
    get: () => store.rightDetailsOpenSize,
    set: (size: string) => store.setRightDetailsOpenSize(size),
  })

  // ============= 布局操作 =============

  /**
   * 切换侧边栏显示/隐藏
   */
  function toggleSidebar() {
    store.setSidebarOpen(!store.sidebarOpen)
  }

  /**
   * 切换详情面板显示/隐藏
   */
  function toggleDetails() {
    store.setDetailsOpen(!store.detailsOpen)
  }

  /**
   * 设置侧边栏状态
   */
  function setSidebarOpen(open: boolean) {
    store.setSidebarOpen(open)
  }

  /**
   * 设置详情面板状态
   */
  function setDetailsOpen(open: boolean) {
    store.setDetailsOpen(open)
  }

  return {
    // 状态
    isSidebarOpen,
    isDetailsOpen,
    leftSidebarSize,
    rightDetailsSize,
    leftSidebarOpenSize,
    rightDetailsOpenSize,

    // 操作
    toggleSidebar,
    toggleDetails,
    setSidebarOpen,
    setDetailsOpen,
  }
}
