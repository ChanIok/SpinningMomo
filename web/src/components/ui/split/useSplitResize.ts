import { computed, ref } from 'vue'
import type { Ref } from 'vue'

interface UseSplitResizeOptions {
  direction: Ref<'horizontal' | 'vertical'>
  dividerSize: Ref<number>
  min: Ref<number | string>
  max: Ref<number | string>
  onUpdate: (size: number | string) => void
  onDragStart?: (e: MouseEvent) => void
  onDragEnd?: (e: MouseEvent) => void
}

export function useSplitResize(options: UseSplitResizeOptions) {
  const { direction, dividerSize, min, max, onUpdate, onDragStart, onDragEnd } = options

  const containerRef = ref<HTMLElement>()
  const dividerRef = ref<HTMLElement>()
  const isDragging = ref(false)
  let startOffset = 0

  /**
   * 将字符串尺寸转换为像素值
   */
  function parseSizeToPixels(size: number | string, containerSize: number): number {
    if (typeof size === 'number') {
      return size * containerSize
    }
    // 处理像素值 "200px"
    const px = parseFloat(size)
    return isNaN(px) ? 0 : px
  }

  /**
   * 计算新的面板尺寸
   */
  function calculateNewSize(e: MouseEvent, currentSize: number | string): number | string {
    const container = containerRef.value
    if (!container) return currentSize

    const containerRect = container.getBoundingClientRect()
    const isHorizontal = direction.value === 'horizontal'

    // 容器可用尺寸（减去分隔条宽度）
    const containerUsableSize = isHorizontal
      ? containerRect.width - dividerSize.value
      : containerRect.height - dividerSize.value

    // 计算鼠标相对位置
    const mousePosition = isHorizontal
      ? e.clientX - containerRect.left - startOffset
      : e.clientY - containerRect.top + startOffset // 垂直方向用加法

    // 计算 min/max 的像素值
    const minPx = parseSizeToPixels(min.value, containerUsableSize)
    const maxPx = parseSizeToPixels(max.value, containerUsableSize)

    // 应用限制
    let newPx = Math.max(minPx, Math.min(mousePosition, maxPx, containerUsableSize))

    // 根据当前尺寸类型返回对应格式
    if (typeof currentSize === 'string' && currentSize.endsWith('px')) {
      return `${newPx}px`
    } else {
      // 返回百分比（0-1）
      return newPx / containerUsableSize
    }
  }

  /**
   * 鼠标按下处理
   */
  function handleMouseDown(e: MouseEvent, currentSize: number | string) {
    e.preventDefault()

    const divider = dividerRef.value
    if (!divider) return

    // 记录初始偏移量
    const dividerRect = divider.getBoundingClientRect()
    const isHorizontal = direction.value === 'horizontal'

    if (isHorizontal) {
      startOffset = e.clientX - dividerRect.left
    } else {
      // 垂直方向：offset 需要反过来（与 Naive UI 保持一致）
      startOffset = dividerRect.top - e.clientY
    }

    isDragging.value = true
    onDragStart?.(e)

    // 设置全局光标
    const cursor = isHorizontal ? 'col-resize' : 'row-resize'
    document.body.style.cursor = cursor
    document.body.style.userSelect = 'none'

    // 鼠标移动处理
    const handleMouseMove = (e: MouseEvent) => {
      const newSize = calculateNewSize(e, currentSize)
      onUpdate(newSize)
    }

    // 鼠标释放处理
    const handleMouseUp = (e: MouseEvent) => {
      window.removeEventListener('mousemove', handleMouseMove)
      window.removeEventListener('mouseup', handleMouseUp)

      isDragging.value = false
      document.body.style.cursor = ''
      document.body.style.userSelect = ''

      onDragEnd?.(e)
    }

    window.addEventListener('mousemove', handleMouseMove)
    window.addEventListener('mouseup', handleMouseUp)

    // 立即更新一次尺寸，确保初始响应
    const newSize = calculateNewSize(e, currentSize)
    onUpdate(newSize)
  }

  /**
   * 计算第一个面板的样式
   */
  function getFirstPaneStyle(size: number | string) {
    if (typeof size === 'string' && size.endsWith('px')) {
      return { flex: `0 0 ${size}` }
    } else if (typeof size === 'number') {
      const percentage = size * 100
      const offset = dividerSize.value * size
      return { flex: `0 0 calc(${percentage}% - ${offset}px)` }
    }
    return { flex: `0 0 50%` }
  }

  /**
   * 计算分隔条样式
   */
  const dividerStyle = computed(() => {
    const isHorizontal = direction.value === 'horizontal'
    return isHorizontal
      ? { width: `${dividerSize.value}px`, height: '100%' }
      : { width: '100%', height: `${dividerSize.value}px` }
  })

  /**
   * 计算分隔条光标样式
   */
  const dividerCursor = computed(() => {
    return direction.value === 'horizontal' ? 'cursor-col-resize' : 'cursor-row-resize'
  })

  return {
    containerRef,
    dividerRef,
    isDragging,
    dividerStyle,
    dividerCursor,
    handleMouseDown,
    getFirstPaneStyle,
  }
}
