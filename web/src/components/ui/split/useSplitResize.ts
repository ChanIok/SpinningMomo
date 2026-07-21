import { computed, ref } from 'vue'
import type { Ref } from 'vue'

interface UseSplitResizeOptions {
  direction: Ref<'horizontal' | 'vertical'>
  dividerSize: Ref<number>
  min: Ref<number | string>
  max: Ref<number | string>
  reverse: Ref<boolean>
  onUpdate: (size: number | string) => void
  onDrag?: (e: MouseEvent) => void
  onDragStart?: (e: MouseEvent) => void
  onDragEnd?: (e: MouseEvent) => void
}

export function useSplitResize(options: UseSplitResizeOptions) {
  const { direction, dividerSize, min, max, reverse, onUpdate, onDrag, onDragStart, onDragEnd } =
    options

  const containerRef = ref<HTMLElement>()
  const dividerRef = ref<HTMLElement>()
  const isDragging = ref(false)
  let startOffset = 0

  function parseSizeToPixels(size: number | string, containerSize: number): number {
    if (typeof size === 'number') {
      return size * containerSize
    }

    const px = parseFloat(size)
    return Number.isNaN(px) ? 0 : px
  }

  function calculateNewSize(e: MouseEvent, currentSize: number | string): number | string {
    const container = containerRef.value
    if (!container) return currentSize

    const containerRect = container.getBoundingClientRect()
    const isHorizontal = direction.value === 'horizontal'
    // 覆盖式 divider 不占布局宽度，面板可分配尺寸直接等于容器尺寸。
    const containerUsableSize = isHorizontal ? containerRect.width : containerRect.height

    let mousePosition: number

    if (reverse.value) {
      if (isHorizontal) {
        mousePosition = containerRect.right - e.clientX + startOffset
      } else {
        mousePosition = containerRect.bottom - e.clientY - startOffset
      }
    } else if (isHorizontal) {
      mousePosition = e.clientX - containerRect.left - startOffset
    } else {
      mousePosition = e.clientY - containerRect.top + startOffset
    }

    const minPx = parseSizeToPixels(min.value, containerUsableSize)
    const maxPx = parseSizeToPixels(max.value, containerUsableSize)
    const newPx = Math.max(minPx, Math.min(mousePosition, maxPx, containerUsableSize))

    if (typeof currentSize === 'string' && currentSize.endsWith('px')) {
      return `${newPx}px`
    }

    return newPx / containerUsableSize
  }

  function handleMouseDown(e: MouseEvent, currentSize: number | string) {
    e.preventDefault()

    const divider = dividerRef.value
    if (!divider) return

    const dividerRect = divider.getBoundingClientRect()
    const isHorizontal = direction.value === 'horizontal'

    if (reverse.value) {
      if (isHorizontal) {
        startOffset = dividerRect.right - e.clientX
      } else {
        startOffset = e.clientY - dividerRect.bottom
      }
    } else if (isHorizontal) {
      startOffset = e.clientX - dividerRect.left
    } else {
      startOffset = dividerRect.top - e.clientY
    }

    isDragging.value = true
    onDragStart?.(e)

    document.body.style.cursor = isHorizontal ? 'ew-resize' : 'ns-resize'
    document.body.style.userSelect = 'none'

    const handleMouseMove = (event: MouseEvent) => {
      const newSize = calculateNewSize(event, currentSize)
      onUpdate(newSize)
      onDrag?.(event)
    }

    const handleMouseUp = (event: MouseEvent) => {
      window.removeEventListener('mousemove', handleMouseMove)
      window.removeEventListener('mouseup', handleMouseUp)

      isDragging.value = false
      document.body.style.cursor = ''
      document.body.style.userSelect = ''

      onDragEnd?.(event)
    }

    window.addEventListener('mousemove', handleMouseMove)
    window.addEventListener('mouseup', handleMouseUp)

    const newSize = calculateNewSize(e, currentSize)
    onUpdate(newSize)
  }

  function getFirstPaneStyle(size: number | string) {
    if (reverse.value) {
      return { flex: '1' }
    }

    if (typeof size === 'string' && size.endsWith('px')) {
      return { flex: `0 0 ${size}` }
    }

    if (typeof size === 'number') {
      const percentage = size * 100
      return { flex: `0 0 ${percentage}%` }
    }

    return { flex: '0 0 50%' }
  }

  function getSecondPaneStyle(size: number | string) {
    if (!reverse.value) {
      return { flex: '1' }
    }

    // reverse 模式下，size 表示第二个面板的固定尺寸，
    // 适合“主区域自适应 + 右侧栏固定宽度”的布局。
    if (typeof size === 'string' && size.endsWith('px')) {
      return { flex: `0 0 ${size}` }
    }

    if (typeof size === 'number') {
      const percentage = size * 100
      return { flex: `0 0 ${percentage}%` }
    }

    return { flex: '0 0 50%' }
  }

  const dividerStyle = computed(() => {
    const isHorizontal = direction.value === 'horizontal'
    return isHorizontal
      ? { width: `${dividerSize.value}px`, height: '100%' }
      : { width: '100%', height: `${dividerSize.value}px` }
  })

  const dividerCursor = computed(() => {
    return direction.value === 'horizontal' ? 'cursor-ew-resize' : 'cursor-ns-resize'
  })

  return {
    containerRef,
    dividerRef,
    isDragging,
    dividerStyle,
    dividerCursor,
    handleMouseDown,
    getFirstPaneStyle,
    getSecondPaneStyle,
  }
}
