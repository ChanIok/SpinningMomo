import { useCallback, useEffect, useMemo } from 'react'
import { useGalleryStore } from '@/lib/gallery/galleryStore'
import type { Asset } from '@/lib/gallery/types'

export function useGalleryLightbox() {
  // 从 store 获取状态和操作
  const lightbox = useGalleryStore((state) => state.lightbox)
  const assets = useGalleryStore((state) => state.assets)
  const openLightbox = useGalleryStore((state) => state.openLightbox)
  const closeLightbox = useGalleryStore((state) => state.closeLightbox)
  const goToLightboxIndex = useGalleryStore((state) => state.goToLightboxIndex)
  const goToPreviousLightbox = useGalleryStore((state) => state.goToPreviousLightbox)
  const goToNextLightbox = useGalleryStore((state) => state.goToNextLightbox)

  // 计算当前资产和相关状态
  const currentAsset = useMemo(() => {
    if (!lightbox.isOpen || lightbox.assets.length === 0) return null
    return lightbox.assets[lightbox.currentIndex]
  }, [lightbox.isOpen, lightbox.assets, lightbox.currentIndex])

  const canGoToPrevious = lightbox.isOpen && lightbox.currentIndex > 0
  const canGoToNext = lightbox.isOpen && lightbox.currentIndex < lightbox.assets.length - 1

  // 打开 Lightbox 并显示指定资产
  const openLightboxWithAsset = useCallback(
    (asset: Asset) => {
      const assetIndex = assets.findIndex((a) => a.id === asset.id)
      if (assetIndex !== -1) {
        openLightbox(assets, assetIndex)
      }
    },
    [assets, openLightbox]
  )

  // 打开 Lightbox 并显示指定索引的资产
  const openLightboxAtIndex = useCallback(
    (index: number) => {
      if (index >= 0 && index < assets.length) {
        openLightbox(assets, index)
      }
    },
    [assets, openLightbox]
  )

  // 跳转到指定资产
  const goToAsset = useCallback(
    (assetId: number) => {
      const assetIndex = lightbox.assets.findIndex((asset) => asset.id === assetId)
      if (assetIndex !== -1) {
        goToLightboxIndex(assetIndex)
      }
    },
    [lightbox.assets, goToLightboxIndex]
  )

  // 跳转到第一个资产
  const goToFirst = useCallback(() => {
    if (lightbox.isOpen && lightbox.assets.length > 0) {
      goToLightboxIndex(0)
    }
  }, [lightbox.isOpen, lightbox.assets.length, goToLightboxIndex])

  // 跳转到最后一个资产
  const goToLast = useCallback(() => {
    if (lightbox.isOpen && lightbox.assets.length > 0) {
      goToLightboxIndex(lightbox.assets.length - 1)
    }
  }, [lightbox.isOpen, lightbox.assets.length, goToLightboxIndex])

  // 预加载相邻图片的逻辑
  const preloadAdjacentImages = useCallback(() => {
    if (!lightbox.isOpen || !currentAsset) return

    const { currentIndex, assets: lightboxAssets } = lightbox
    const preloadIndices = []

    // 预加载前后各2张图片
    for (let offset = -2; offset <= 2; offset++) {
      const index = currentIndex + offset
      if (index >= 0 && index < lightboxAssets.length && index !== currentIndex) {
        preloadIndices.push(index)
      }
    }

    // 创建预加载的 Image 对象
    preloadIndices.forEach((index) => {
      const asset = lightboxAssets[index]
      if (asset) {
        const img = new Image()
        // 这里应该使用实际的图片URL获取逻辑
        img.src = `/api/assets/${asset.id}/file`
      }
    })
  }, [lightbox, currentAsset])

  // 当 lightbox 打开或索引变化时触发预加载
  useEffect(() => {
    if (lightbox.isOpen) {
      preloadAdjacentImages()
    }
  }, [lightbox.isOpen, lightbox.currentIndex, preloadAdjacentImages])

  // 键盘事件处理
  const handleKeyDown = useCallback(
    (event: KeyboardEvent) => {
      if (!lightbox.isOpen) return

      switch (event.key) {
        case 'Escape':
          event.preventDefault()
          closeLightbox()
          break
        case 'ArrowLeft':
        case 'ArrowUp':
          event.preventDefault()
          if (canGoToPrevious) {
            goToPreviousLightbox()
          }
          break
        case 'ArrowRight':
        case 'ArrowDown':
        case ' ': // Space
          event.preventDefault()
          if (canGoToNext) {
            goToNextLightbox()
          }
          break
        case 'Home':
          event.preventDefault()
          goToFirst()
          break
        case 'End':
          event.preventDefault()
          goToLast()
          break
        // 数字键快速跳转（1-9对应前9张图）
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
          const index = parseInt(event.key) - 1
          if (index < lightbox.assets.length) {
            event.preventDefault()
            goToLightboxIndex(index)
          }
          break
        }
      }
    },
    [
      lightbox.isOpen,
      lightbox.assets.length,
      canGoToPrevious,
      canGoToNext,
      closeLightbox,
      goToPreviousLightbox,
      goToNextLightbox,
      goToFirst,
      goToLast,
      goToLightboxIndex,
    ]
  )

  // 注册全局键盘事件监听
  useEffect(() => {
    if (lightbox.isOpen) {
      document.addEventListener('keydown', handleKeyDown)
      return () => document.removeEventListener('keydown', handleKeyDown)
    }
  }, [lightbox.isOpen, handleKeyDown])

  // 滑动手势支持（移动端）
  const getSwipeHandlers = useCallback(() => {
    let startX = 0
    let startY = 0

    const handleTouchStart = (e: TouchEvent) => {
      if (!lightbox.isOpen) return
      startX = e.touches[0].clientX
      startY = e.touches[0].clientY
    }

    const handleTouchEnd = (e: TouchEvent) => {
      if (!lightbox.isOpen) return

      const endX = e.changedTouches[0].clientX
      const endY = e.changedTouches[0].clientY
      const deltaX = endX - startX
      const deltaY = endY - startY

      // 只响应水平滑动，且滑动距离足够大
      if (Math.abs(deltaX) > Math.abs(deltaY) && Math.abs(deltaX) > 50) {
        if (deltaX > 0 && canGoToPrevious) {
          goToPreviousLightbox()
        } else if (deltaX < 0 && canGoToNext) {
          goToNextLightbox()
        }
      }
    }

    return {
      onTouchStart: handleTouchStart,
      onTouchEnd: handleTouchEnd,
    }
  }, [lightbox.isOpen, canGoToPrevious, canGoToNext, goToPreviousLightbox, goToNextLightbox])

  // 获取资产在当前 lightbox 中的位置信息
  const getCurrentPosition = useCallback(() => {
    if (!lightbox.isOpen || !currentAsset) return null
    return {
      current: lightbox.currentIndex + 1,
      total: lightbox.assets.length,
      asset: currentAsset,
    }
  }, [lightbox.isOpen, lightbox.currentIndex, lightbox.assets.length, currentAsset])

  // 检查是否可以执行导航操作
  const canNavigate = lightbox.isOpen && lightbox.assets.length > 1

  return {
    // 状态
    lightbox,
    currentAsset,
    canGoToPrevious,
    canGoToNext,
    canNavigate,

    // 基础操作
    openLightboxWithAsset,
    openLightboxAtIndex,
    closeLightbox,
    goToPreviousLightbox,
    goToNextLightbox,

    // 高级操作
    goToAsset,
    goToFirst,
    goToLast,
    goToLightboxIndex,

    // 辅助功能
    preloadAdjacentImages,
    getCurrentPosition,
    getSwipeHandlers,

    // 便捷方法
    isOpen: lightbox.isOpen,
    isEmpty: lightbox.assets.length === 0,
    isFirst: lightbox.currentIndex === 0,
    isLast: lightbox.currentIndex === lightbox.assets.length - 1,
  }
}
