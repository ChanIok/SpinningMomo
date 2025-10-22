import { computed } from 'vue'
import { useGalleryStore } from '../store'

/**
 * Gallery 布局管理 Composable
 * 管理侧边栏和详情面板的显示状态
 */
export function useGalleryLayout() {
  const store = useGalleryStore()

  // ============= 布局状态 =============
  const isSidebarOpen = computed(() => store.sidebar.isOpen)
  const isDetailsOpen = computed(() => store.detailsOpen)

  // ============= 布局操作 =============
  
  /**
   * 切换侧边栏显示/隐藏
   */
  function toggleSidebar() {
    store.setSidebarOpen(!store.sidebar.isOpen)
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
    
    // 操作
    toggleSidebar,
    toggleDetails,
    setSidebarOpen,
    setDetailsOpen,
  }
}

