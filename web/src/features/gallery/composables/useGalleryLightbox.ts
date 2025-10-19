import { useGalleryStore } from '../store'

/**
 * Gallery Lightbox Composable
 * 负责lightbox的业务逻辑：打开/关闭、导航等
 */
export function useGalleryLightbox() {
  const store = useGalleryStore()

  /**
   * 打开Lightbox
   * @param index 要打开的资产的全局索引
   */
  function openLightbox(index: number) {
    store.openLightbox(index)
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
  }

  /**
   * 导航到下一张
   */
  function goToNext() {
    store.goToNextLightbox()
  }

  /**
   * 跳转到指定索引
   */
  function goToIndex(index: number) {
    store.goToLightboxIndex(index)
  }

  /**
   * 关闭Lightbox
   */
  function closeLightbox() {
    store.closeLightbox()
  }

  return {
    openLightbox,
    closeLightbox,
    toggleFullscreen,
    toggleFilmstrip,
    goToPrevious,
    goToNext,
    goToIndex,
  }
}
