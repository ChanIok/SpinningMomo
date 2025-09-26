import { useMemo, useState, useEffect, useCallback } from 'react'
import { useGalleryStore } from '@/lib/gallery/galleryStore'
import type { ViewMode } from '@/lib/gallery/types'

interface UseGalleryViewOptions {
  containerRef?: React.RefObject<HTMLElement>
  headerHeight?: number
}

export function useGalleryView(options: UseGalleryViewOptions = {}) {
  const { headerHeight = 200 } = options

  // 从 store 获取视图配置
  const viewConfig = useGalleryStore((state) => state.viewConfig)
  const setViewConfig = useGalleryStore((state) => state.setViewConfig)

  // 容器尺寸状态
  const [containerSize, setContainerSize] = useState({
    width: 1200,
    height: 800,
  })

  // 根据 size 等级计算基础网格尺寸
  const cellSize = useMemo(() => {
    const sizeMultiplier = (viewConfig.size - 1) * 0.5 + 1 // 1-3倍大小
    const baseSize = 160
    return Math.floor(baseSize * sizeMultiplier) + 16 // +16px for padding
  }, [viewConfig.size])

  // 响应式列数计算
  const columnCount = useMemo(() => {
    const availableWidth = containerSize.width - 32 // 减去容器padding
    return Math.max(2, Math.floor(availableWidth / cellSize))
  }, [containerSize.width, cellSize])

  // 计算行数
  const getRowCount = useCallback(
    (itemCount: number) => {
      return Math.ceil(itemCount / columnCount)
    },
    [columnCount]
  )

  // 视图模式切换
  const switchViewMode = useCallback(
    (mode: ViewMode) => {
      setViewConfig({ mode })
    },
    [setViewConfig]
  )

  // 调整尺寸
  const adjustSize = useCallback(
    (delta: number) => {
      const newSize = Math.max(1, Math.min(5, viewConfig.size + delta))
      setViewConfig({ size: newSize })
    },
    [viewConfig.size, setViewConfig]
  )

  // 容器尺寸更新
  const updateContainerSize = useCallback(() => {
    setContainerSize({
      width: window.innerWidth,
      height: window.innerHeight - headerHeight,
    })
  }, [headerHeight])

  // 监听窗口尺寸变化
  useEffect(() => {
    updateContainerSize()
    window.addEventListener('resize', updateContainerSize)
    return () => window.removeEventListener('resize', updateContainerSize)
  }, [updateContainerSize])

  // 获取网格样式配置
  const getGridConfig = useMemo(
    () => ({
      columnCount,
      columnWidth: cellSize,
      rowHeight: cellSize,
      containerWidth: containerSize.width,
      containerHeight: containerSize.height,
    }),
    [columnCount, cellSize, containerSize]
  )

  // 获取瀑布流配置
  const getMasonryConfig = useMemo(
    () => ({
      columnCount: Math.max(2, Math.min(6, Math.floor(containerSize.width / 280))),
      columnWidth: Math.floor(
        (containerSize.width - 32) / Math.max(2, Math.min(6, Math.floor(containerSize.width / 280)))
      ),
      gutter: 16,
    }),
    [containerSize.width]
  )

  // 判断是否为移动端视图
  const isMobile = useMemo(() => {
    return containerSize.width < 768
  }, [containerSize.width])

  // 自适应视图模式
  const adaptiveViewMode: ViewMode = useMemo(() => {
    if (isMobile) return 'list'
    if (containerSize.width < 1024) return 'grid'
    return 'masonry'
  }, [isMobile, containerSize.width])

  return {
    // 状态
    viewConfig,
    containerSize,
    cellSize,
    columnCount,
    isMobile,
    adaptiveViewMode,

    // 配置
    getGridConfig,
    getMasonryConfig,

    // 操作
    switchViewMode,
    adjustSize,
    updateContainerSize,
    getRowCount,

    // 便捷方法
    canIncreaseSize: viewConfig.size < 5,
    canDecreaseSize: viewConfig.size > 1,
    increaseSize: () => adjustSize(1),
    decreaseSize: () => adjustSize(-1),
  }
}
