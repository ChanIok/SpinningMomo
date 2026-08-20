import { computed, onBeforeUnmount, ref } from 'vue'
import type { Ref } from 'vue'

type ResizeEvent = MouseEvent | TouchEvent

interface UseSplitResizeOptions {
  direction: Ref<'horizontal' | 'vertical'>
  dividerSize: Ref<number>
  min: Ref<number | string>
  max: Ref<number | string>
  reverse: Ref<boolean>
  onUpdate: (size: number | string) => void
  onDrag?: (e: ResizeEvent) => void
  onDragStart?: (e: ResizeEvent) => void
  onDragEnd?: (e: ResizeEvent) => void
}

interface ResizeCoordinates {
  clientX: number
  clientY: number
}

interface DragSession {
  startCoordinate: number
  startSizePx: number
  previousCursor: string
  previousUserSelect: string
}

export function useSplitResize(options: UseSplitResizeOptions) {
  const { direction, dividerSize, min, max, reverse, onUpdate, onDrag, onDragStart, onDragEnd } =
    options

  const containerRef = ref<HTMLElement>()
  const dividerRef = ref<HTMLElement>()
  const isDragging = ref(false)
  let dragSession: DragSession | null = null
  let removeDragListeners: (() => void) | null = null

  function parseSizeToPixels(size: number | string, containerSize: number): number {
    if (typeof size === 'number') {
      return size * containerSize
    }

    const normalized = size.trim()
    const value = parseFloat(normalized)
    if (Number.isNaN(value)) return 0

    return normalized.endsWith('%') ? (value / 100) * containerSize : value
  }

  function getCoordinates(event: ResizeEvent): ResizeCoordinates | null {
    if ('touches' in event) {
      const touch = event.touches[0] ?? event.changedTouches[0]
      return touch ? { clientX: touch.clientX, clientY: touch.clientY } : null
    }

    return event
  }

  function getAxisCoordinate(coords: ResizeCoordinates): number {
    return direction.value === 'horizontal' ? coords.clientX : coords.clientY
  }

  function getContainerUsableSize(rect: DOMRect): number {
    return direction.value === 'horizontal' ? rect.width : rect.height
  }

  function beginDrag(coords: ResizeCoordinates, currentSize: number | string): boolean {
    const container = containerRef.value
    if (!container) return false

    const containerSize = getContainerUsableSize(container.getBoundingClientRect())
    if (containerSize <= 0) return false

    dragSession = {
      startCoordinate: getAxisCoordinate(coords),
      startSizePx: parseSizeToPixels(currentSize, containerSize),
      previousCursor: document.body.style.cursor,
      previousUserSelect: document.body.style.userSelect,
    }
    isDragging.value = true

    const isHorizontal = direction.value === 'horizontal'
    document.body.style.cursor = isHorizontal ? 'ew-resize' : 'ns-resize'
    document.body.style.userSelect = 'none'
    return true
  }

  function calculateNewSize(
    coords: { clientX: number; clientY: number },
    currentSize: number | string
  ): number | string {
    const container = containerRef.value
    if (!container || !dragSession) return currentSize

    const containerRect = container.getBoundingClientRect()
    // 覆盖式 divider 不占布局宽度，面板可分配尺寸直接等于容器尺寸。
    const containerUsableSize = getContainerUsableSize(containerRect)
    if (containerUsableSize <= 0) return currentSize

    const delta = getAxisCoordinate(coords) - dragSession.startCoordinate
    const position = dragSession.startSizePx + (reverse.value ? -delta : delta)

    const minPx = parseSizeToPixels(min.value, containerUsableSize)
    const maxPx = parseSizeToPixels(max.value, containerUsableSize)
    const newPx = Math.max(minPx, Math.min(position, maxPx, containerUsableSize))

    if (typeof currentSize === 'string' && currentSize.endsWith('px')) {
      return `${newPx}px`
    }

    return newPx / containerUsableSize
  }

  function handleMouseDown(e: MouseEvent, currentSize: number | string) {
    if (e.button !== 0 || dragSession) return
    e.preventDefault()

    const coords = getCoordinates(e)
    if (!coords || !beginDrag(coords, currentSize)) return
    onDragStart?.(e)

    const handleMouseMove = (event: MouseEvent) => {
      const coords = getCoordinates(event)
      if (!coords) return

      const newSize = calculateNewSize(coords, currentSize)
      onUpdate(newSize)
      onDrag?.(event)
    }

    const handleMouseUp = (event: MouseEvent) => {
      finishDrag(event)
    }

    window.addEventListener('mousemove', handleMouseMove)
    window.addEventListener('mouseup', handleMouseUp)
    removeDragListeners = () => {
      window.removeEventListener('mousemove', handleMouseMove)
      window.removeEventListener('mouseup', handleMouseUp)
    }
  }

  function handleTouchStart(e: TouchEvent, currentSize: number | string) {
    if (e.touches.length !== 1 || dragSession) return
    e.preventDefault()

    const coords = getCoordinates(e)
    if (!coords || !beginDrag(coords, currentSize)) return
    onDragStart?.(e)

    const handleTouchMove = (event: TouchEvent) => {
      if (event.touches.length !== 1) return
      event.preventDefault()
      const coords = getCoordinates(event)
      if (!coords) return

      const newSize = calculateNewSize(coords, currentSize)
      onUpdate(newSize)
      onDrag?.(event)
    }

    const handleTouchEnd = (event: TouchEvent) => {
      finishDrag(event)
    }

    window.addEventListener('touchmove', handleTouchMove, { passive: false })
    window.addEventListener('touchend', handleTouchEnd)
    window.addEventListener('touchcancel', handleTouchEnd)
    removeDragListeners = () => {
      window.removeEventListener('touchmove', handleTouchMove)
      window.removeEventListener('touchend', handleTouchEnd)
      window.removeEventListener('touchcancel', handleTouchEnd)
    }
  }

  function finishDrag(event: ResizeEvent) {
    const session = dragSession
    removeDragListeners?.()
    removeDragListeners = null
    dragSession = null
    isDragging.value = false

    if (session) {
      document.body.style.cursor = session.previousCursor
      document.body.style.userSelect = session.previousUserSelect
    }

    onDragEnd?.(event)
  }

  function cleanupDrag() {
    const session = dragSession
    removeDragListeners?.()
    removeDragListeners = null
    dragSession = null
    isDragging.value = false

    if (session) {
      document.body.style.cursor = session.previousCursor
      document.body.style.userSelect = session.previousUserSelect
    }
  }

  onBeforeUnmount(cleanupDrag)

  function getFirstPaneStyle(size: number | string) {
    if (reverse.value) {
      return { flex: '1' }
    }

    if (typeof size === 'string') {
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
    if (typeof size === 'string') {
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
    handleTouchStart,
    getFirstPaneStyle,
    getSecondPaneStyle,
  }
}
