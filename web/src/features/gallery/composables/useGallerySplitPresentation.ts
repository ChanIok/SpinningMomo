import { computed } from 'vue'
import { useGalleryStore } from '../store'

const TOUCH_DIVIDER_SIZE = 12
const DEFAULT_DIVIDER_SIZE = 5

/**
 * 统一图库 Split 的触摸提示与命中区域。
 * 输入模式由图库交互状态决定，Split 组件本身只负责渲染和拖拽。
 */
export function useGallerySplitPresentation() {
  const store = useGalleryStore()

  const showHandle = computed(() => store.isTouchMode)
  const dividerSize = computed(() =>
    store.isTouchMode ? TOUCH_DIVIDER_SIZE : DEFAULT_DIVIDER_SIZE
  )

  return {
    showHandle,
    dividerSize,
  }
}
