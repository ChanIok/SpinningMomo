import { onMounted, onUnmounted } from 'vue'
import { useGalleryStore } from '../store'
import { useGalleryView } from './useGalleryView'
import { galleryApi } from '../api'
import type { Asset } from '../types'

/**
 * Gallery Lightbox Composable
 * 负责lightbox的业务逻辑：打开/关闭、导航、键盘事件、预加载等
 */
export function useGalleryLightbox() {
  const store = useGalleryStore()
  const galleryView = useGalleryView()

  /**
   * 打开Lightbox
   * @param asset 要打开的资产
   */
  function openLightbox(asset: Asset) {
    // 使用当前筛选和排序后的资产列表（与GridView一致）
    const assets = galleryView.sortedAssets.value
    const startIndex = assets.findIndex((a) => a.id === asset.id)

    if (startIndex === -1) {
      console.error('Asset not found in current view')
      return
    }

    store.openLightbox(assets, startIndex)
    preloadAdjacentImages()
  }

  /**
   * 切换全屏模式
   */
  function toggleFullscreen() {
    if (!store.lightbox.isFullscreen) {
      // 进入全屏
      document.documentElement.requestFullscreen?.()
    } else {
      // 退出全屏
      document.exitFullscreen?.()
    }
    store.toggleLightboxFullscreen()
  }

  /**
   * 切换Filmstrip显示
   */
  function toggleFilmstrip() {
    store.toggleLightboxFilmstrip()
  }

  /**
   * 导航到上一张
   */
  function goToPrevious() {
    store.goToPreviousLightbox()
    preloadAdjacentImages()
  }

  /**
   * 导航到下一张
   */
  function goToNext() {
    store.goToNextLightbox()
    preloadAdjacentImages()
  }

  /**
   * 跳转到指定索引
   */
  function goToIndex(index: number) {
    store.goToLightboxIndex(index)
    preloadAdjacentImages()
  }

  /**
   * 预加载前后几张图片
   */
  function preloadAdjacentImages() {
    const { currentIndex, assets } = store.lightbox
    const preloadCount = 2 // 前后各预加载2张

    for (let i = -preloadCount; i <= preloadCount; i++) {
      const index = currentIndex + i
      if (index >= 0 && index < assets.length && i !== 0) {
        const asset = assets[index]
        if (asset) {
          // TODO: 等待后端原图API
          // const img = new Image()
          // img.src = galleryApi.getAssetOriginalUrl(asset)

          // 暂时预加载缩略图
          const img = new Image()
          img.src = galleryApi.getAssetThumbnailUrl(asset)
        }
      }
    }
  }

  /**
   * 关闭Lightbox
   */
  function closeLightbox() {
    store.closeLightbox()
  }

  /**
   * 处理键盘事件
   */
  function handleKeyboard(event: KeyboardEvent) {
    if (!store.lightbox.isOpen) return

    switch (event.key) {
      case 'Escape':
        closeLightbox()
        break
      case 'ArrowLeft':
        goToPrevious()
        break
      case 'ArrowRight':
        goToNext()
        break
      case 'f':
      case 'F':
        event.preventDefault()
        toggleFullscreen()
        break
      case 'Tab':
        event.preventDefault()
        toggleFilmstrip()
        break
      case ' ':
        event.preventDefault()
        // 空格键：切换暂停/播放（如果有自动播放功能）
        break
    }
  }

  // 监听键盘事件
  onMounted(() => {
    window.addEventListener('keydown', handleKeyboard)
  })

  onUnmounted(() => {
    window.removeEventListener('keydown', handleKeyboard)
  })

  return {
    openLightbox,
    closeLightbox,
    toggleFullscreen,
    toggleFilmstrip,
    goToPrevious,
    goToNext,
    goToIndex,
    handleKeyboard,
  }
}
