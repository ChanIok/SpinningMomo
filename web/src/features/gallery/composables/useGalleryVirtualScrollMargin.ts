import { computed, type Ref } from 'vue'
import { useElementSize } from '@vueuse/core'
import { useGalleryStore } from '../store'

/**
 * 计算滚动容器顶部到虚拟列表起点的真实距离。
 *
 * 紧凑图库的工具栏占位和信息头都位于虚拟列表之前；TanStack Virtual
 * 需要通过 scrollMargin 感知这段前置内容，而渲染虚拟项时再减回同一段距离。
 */
export function useGalleryVirtualScrollMargin(
  scrollContainerRef: Ref<HTMLElement | null>,
  headerRef: Ref<HTMLElement | null>,
  compactToolbarHeight: Ref<number>
) {
  const store = useGalleryStore()
  const { height: headerHeight } = useElementSize(headerRef)

  const scrollMargin = computed(() => {
    const container = scrollContainerRef.value
    const readContainerPaddingTop = () =>
      typeof window !== 'undefined' && container
        ? Number.parseFloat(window.getComputedStyle(container).paddingTop) || 0
        : 0

    // 紧凑工具栏高度由外层 ResizeObserver 提供；首次测量完成前回读 CSS fallback，
    // 桌面端则保留滚动容器自身的 py-2 顶部内边距。
    const containerPaddingTop =
      store.isCompactWindow && compactToolbarHeight.value > 0
        ? compactToolbarHeight.value
        : readContainerPaddingTop()

    return containerPaddingTop + (store.isCompactWindow ? headerHeight.value : 0)
  })

  return { scrollMargin }
}
