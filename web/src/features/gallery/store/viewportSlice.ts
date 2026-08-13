import { computed, ref } from 'vue'
import { GALLERY_WINDOW_COMPACT_BREAKPOINT } from '../constants'

/**
 * 窗口级响应式状态。
 *
 * 这里的宽度指应用窗口，而不是图库中间栏或暗房内容区；内容区的局部压缩由组件自行判断。
 */
export function createViewportSlice() {
  const windowWidth = ref(0)
  const isCompactWindow = computed(
    () => windowWidth.value > 0 && windowWidth.value < GALLERY_WINDOW_COMPACT_BREAKPOINT
  )

  function setWindowWidth(width: number) {
    windowWidth.value = Math.max(0, Math.round(width))
  }

  function resetViewportState() {
    windowWidth.value = 0
  }

  return {
    windowWidth,
    isCompactWindow,
    setWindowWidth,
    resetViewportState,
  }
}
