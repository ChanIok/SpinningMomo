import { onBeforeUnmount, watch, type Ref } from 'vue'
import { useEventListener } from '@vueuse/core'

interface UseGalleryPinchZoomOptions {
  /** 接收图库触摸手势的外层内容区。 */
  surfaceRef: Ref<HTMLElement | null>
  /** 只有紧凑缩略图视图才允许接管双指手势。 */
  enabled: Readonly<Ref<boolean>>
  /** 读取当前视图尺寸，避免手势开始时依赖组件内部实现。 */
  getSize: () => number
  /** 读取当前布局的动态有效下限。 */
  getMinSize: () => number
  /** 读取当前布局的动态有效上限。 */
  getMaxSize: () => number
  /** 写入视图尺寸，复用图库已有的上下限与持久化语义。 */
  setSize: (size: number) => void
}

interface PointerPosition {
  x: number
  y: number
}

// 两指距离变化小于这个值时只保持手势，不触发布局重排，避免轻触抖动改变尺寸。
const PINCH_MOVE_THRESHOLD = 8
// 缩放结束后短暂吞掉点击，避免双指手势误触素材选择或灯箱。
const CLICK_SUPPRESS_DURATION = 350

function isExcludedTarget(target: EventTarget | null): boolean {
  if (!(target instanceof Element)) {
    return false
  }

  // 时间线轨道自带 pointer capture；表单控件也不应被图库缩放手势接管。
  return Boolean(
    target.closest('.timeline-scrollbar, button, input, textarea, select, [contenteditable="true"]')
  )
}

function getDistance(first: PointerPosition, second: PointerPosition): number {
  return Math.hypot(second.x - first.x, second.y - first.y)
}

function clampViewSize(size: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, size))
}

export function useGalleryPinchZoom(options: UseGalleryPinchZoomOptions) {
  const { surfaceRef, enabled, getSize, getMinSize, getMaxSize, setSize } = options
  const pointers = new Map<number, PointerPosition>()

  let pinchActive = false
  let pinchMoved = false
  let startDistance = 0
  let startSize = 0
  let pendingSize: number | null = null
  let lastQueuedSize: number | null = null
  let animationFrame: number | null = null
  let suppressClickUntil = 0

  function getPinchDistance(): number | null {
    const positions = [...pointers.values()]
    if (positions.length < 2) {
      return null
    }

    return getDistance(positions[0]!, positions[1]!)
  }

  function cancelSizeFrame() {
    if (animationFrame === null) {
      return
    }

    cancelAnimationFrame(animationFrame)
    animationFrame = null
  }

  function flushPendingSize() {
    cancelSizeFrame()

    if (!pinchActive || pendingSize === null || !enabled.value) {
      pendingSize = null
      return
    }

    const nextSize = pendingSize
    pendingSize = null
    if (nextSize !== getSize()) {
      setSize(nextSize)
    }
  }

  function scheduleSizeUpdate(nextSize: number) {
    if (nextSize === lastQueuedSize) {
      return
    }

    lastQueuedSize = nextSize
    pendingSize = nextSize
    if (animationFrame !== null) {
      return
    }

    animationFrame = requestAnimationFrame(() => {
      animationFrame = null
      if (!pinchActive || !enabled.value || pendingSize === null) {
        pendingSize = null
        return
      }

      const size = pendingSize
      pendingSize = null
      if (size !== getSize()) {
        setSize(size)
      }
    })
  }

  function capturePointer(pointerId: number) {
    const surface = surfaceRef.value
    if (!surface || !surface.setPointerCapture) {
      return
    }

    try {
      surface.setPointerCapture(pointerId)
    } catch {
      // 指针可能已经在事件循环中结束；丢失 capture 不应中断图库交互。
    }
  }

  function releaseCapturedPointers() {
    const surface = surfaceRef.value
    if (!surface || !surface.releasePointerCapture) {
      return
    }

    for (const pointerId of pointers.keys()) {
      if (!surface.hasPointerCapture(pointerId)) {
        continue
      }

      try {
        surface.releasePointerCapture(pointerId)
      } catch {
        // 指针已经被系统回收时，释放 capture 可能失败；清理本地状态即可。
      }
    }
  }

  function resetGesture(suppressClick: boolean) {
    if (pinchActive) {
      flushPendingSize()
      if (suppressClick) {
        suppressClickUntil = performance.now() + CLICK_SUPPRESS_DURATION
      }
    } else {
      cancelSizeFrame()
      pendingSize = null
    }

    releaseCapturedPointers()
    pointers.clear()
    pinchActive = false
    pinchMoved = false
    startDistance = 0
    startSize = 0
    lastQueuedSize = null
  }

  function handlePointerDown(event: PointerEvent) {
    if (
      !enabled.value ||
      event.pointerType !== 'touch' ||
      isExcludedTarget(event.target) ||
      pointers.size >= 2
    ) {
      return
    }

    pointers.set(event.pointerId, { x: event.clientX, y: event.clientY })
    capturePointer(event.pointerId)

    if (pointers.size !== 2) {
      return
    }

    const distance = getPinchDistance()
    if (distance === null) {
      resetGesture(false)
      return
    }

    startDistance = Math.max(1, distance)
    startSize = getSize()
    lastQueuedSize = startSize
    pinchActive = true
    pinchMoved = false
    suppressClickUntil = Number.POSITIVE_INFINITY
    event.preventDefault()
  }

  function handlePointerMove(event: PointerEvent) {
    const pointer = pointers.get(event.pointerId)
    if (!pointer) {
      return
    }

    pointer.x = event.clientX
    pointer.y = event.clientY

    if (!pinchActive) {
      return
    }

    const distance = getPinchDistance()
    if (distance === null) {
      return
    }

    // 双指出现后阻止滚动；单指阶段从不调用 preventDefault，保留原生滚动体验。
    event.preventDefault()

    if (!pinchMoved && Math.abs(distance - startDistance) < PINCH_MOVE_THRESHOLD) {
      return
    }

    pinchMoved = true
    scheduleSizeUpdate(
      clampViewSize(Math.round(startSize * (distance / startDistance)), getMinSize(), getMaxSize())
    )
  }

  function handlePointerEnd(event: PointerEvent) {
    if (!pointers.has(event.pointerId)) {
      return
    }

    pointers.delete(event.pointerId)
    if (pinchActive) {
      resetGesture(true)
      return
    }

    releaseCapturedPointers()
    pointers.clear()
  }

  function handleLostPointerCapture(event: PointerEvent) {
    if (pointers.has(event.pointerId)) {
      handlePointerEnd(event)
    }
  }

  function handleClick(event: MouseEvent) {
    if (pinchActive || performance.now() < suppressClickUntil) {
      event.preventDefault()
      event.stopPropagation()
    }
  }

  watch(enabled, (isEnabled) => {
    if (!isEnabled) {
      resetGesture(false)
      suppressClickUntil = 0
    }
  })

  useEventListener(surfaceRef, 'pointerdown', handlePointerDown)
  useEventListener(surfaceRef, 'pointermove', handlePointerMove)
  useEventListener(surfaceRef, 'pointerup', handlePointerEnd)
  useEventListener(surfaceRef, 'pointercancel', handlePointerEnd)
  useEventListener(surfaceRef, 'lostpointercapture', handleLostPointerCapture)
  useEventListener(surfaceRef, 'click', handleClick, { capture: true })
  useEventListener(surfaceRef, 'dblclick', handleClick, { capture: true })

  onBeforeUnmount(() => {
    resetGesture(false)
  })
}
