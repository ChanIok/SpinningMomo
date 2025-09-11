import { useState, useCallback } from 'react'

export function useGalleryLayout() {
  const [isSidebarOpen, setSidebarOpen] = useState(true)
  const [isDetailsOpen, setDetailsOpen] = useState(true)

  // 切换左侧栏的函数 - 现在只需要切换状态，allotment会自动处理布局
  const toggleSidebar = useCallback(() => {
    setSidebarOpen(prev => !prev)
  }, [])

  // 切换右侧详情面板的函数 - 现在只需要切换状态，allotment会自动处理布局
  const toggleDetails = useCallback(() => {
    setDetailsOpen(prev => !prev)
  }, [])

  return {
    isSidebarOpen,
    isDetailsOpen,
    toggleSidebar,
    toggleDetails,
  }
}
