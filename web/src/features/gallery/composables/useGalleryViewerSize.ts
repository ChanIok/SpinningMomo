import { ref, computed, watch, type Ref } from 'vue'
import { useElementSize } from '@vueuse/core'

// 模块级单例缓存：在不同画廊视图（网格/瀑布流/自适应）切换时保留上一次内容区的真实物理尺寸，
// 避免新视图挂载首帧因 ResizeObserver 尚未触发而出现尺寸为 0 的白屏跳闪。
const cachedWidth = ref(0)
const cachedHeight = ref(0)

export function useGalleryViewerSize(containerRef: Ref<HTMLElement | null>) {
  // 挂载时以已知缓存热启动
  const { width: measuredWidth, height: measuredHeight } = useElementSize(containerRef, {
    width: cachedWidth.value,
    height: cachedHeight.value,
  })

  // 真实尺寸变化时更新模块级缓存，供下一个挂载的视图复用
  watch([measuredWidth, measuredHeight], ([w, h]) => {
    if (w > 0) cachedWidth.value = Math.round(w)
    if (h > 0) cachedHeight.value = Math.round(h)
  })

  const width = computed(
    () => measuredWidth.value || cachedWidth.value || containerRef.value?.clientWidth || 0
  )
  const height = computed(
    () => measuredHeight.value || cachedHeight.value || containerRef.value?.clientHeight || 0
  )

  return { width, height }
}
