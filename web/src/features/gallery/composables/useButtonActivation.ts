import { onBeforeUnmount } from 'vue'

type ButtonActivationEvent = MouseEvent | PointerEvent

/** 协调触摸指针激活与浏览器随后派发的兼容点击。 */
export function useButtonActivation() {
  // 只有从按钮上开始且未明显移动的触摸才会激活按钮。
  let activeTouch: {
    target: EventTarget
    pointerId: number
    startX: number
    startY: number
  } | null = null
  let lastTouchTarget: EventTarget | null = null
  let lastTouchAt = 0
  const pendingFrames = new Set<number>()

  function onPointerDown(event: PointerEvent) {
    // 鼠标和手写笔继续使用常规 click 路径。
    if (event.pointerType !== 'touch' || !event.isPrimary || event.button !== 0) {
      return
    }

    activeTouch = {
      target: event.currentTarget as EventTarget,
      pointerId: event.pointerId,
      startX: event.clientX,
      startY: event.clientY,
    }
  }

  function onPointerMove(event: PointerEvent) {
    if (!activeTouch || activeTouch.pointerId !== event.pointerId) {
      return
    }

    // 手指移动超过阈值后视为手势，不再作为按钮点按处理。
    if (Math.hypot(event.clientX - activeTouch.startX, event.clientY - activeTouch.startY) > 10) {
      activeTouch = null
    }
  }

  function onPointerUp(
    event: PointerEvent,
    activate: (event: ButtonActivationEvent) => void,
    deferTouch = false
  ) {
    const target = event.currentTarget as EventTarget
    if (
      event.pointerType !== 'touch' ||
      !activeTouch ||
      activeTouch.target !== target ||
      activeTouch.pointerId !== event.pointerId
    ) {
      return
    }

    // 触摸指针离开按钮后仍可能被隐式捕获到按钮，因此还要检查抬起坐标。
    const button = target as HTMLElement
    const bounds = button.getBoundingClientRect()
    if (
      event.clientX < bounds.left ||
      event.clientX > bounds.right ||
      event.clientY < bounds.top ||
      event.clientY > bounds.bottom
    ) {
      activeTouch = null
      return
    }

    activeTouch = null
    lastTouchTarget = target
    lastTouchAt = performance.now()

    if (!deferTouch) {
      activate(event)
      return
    }

    // 等 pointerup 产生的点击处理完，再挂载抽屉或对话框遮罩。
    const frame = requestAnimationFrame(() => {
      pendingFrames.delete(frame)
      activate(event)
    })
    pendingFrames.add(frame)
  }

  function onPointerCancel(event: PointerEvent) {
    if (activeTouch?.pointerId === event.pointerId) {
      activeTouch = null
    }
  }

  function onClick(event: MouseEvent, activate: (event: ButtonActivationEvent) => void) {
    // 键盘和辅助技术激活没有 pointerdown/pointerup 配对事件。
    if (event.detail === 0) {
      activate(event)
      return
    }

    const target = event.currentTarget as EventTarget
    const pointerType = 'pointerType' in event ? (event as PointerEvent).pointerType : undefined
    // 较旧的浏览器可能把触摸产生的点击报告为没有 pointerType 的 MouseEvent。
    const isTouchCompatibilityClick =
      pointerType === 'touch' ||
      (!pointerType && lastTouchTarget === target && performance.now() - lastTouchAt < 500)

    if (isTouchCompatibilityClick) {
      // 触摸 pointerup 已激活按钮，拦截随后产生的合成点击，避免重复操作或点穿。
      event.preventDefault()
      event.stopPropagation()
      if (lastTouchTarget === target) {
        lastTouchTarget = null
      }
      return
    }

    activate(event)
  }

  onBeforeUnmount(() => {
    // 组件卸载时取消尚未执行的延迟操作。
    for (const frame of pendingFrames) {
      cancelAnimationFrame(frame)
    }
    pendingFrames.clear()
    activeTouch = null
    lastTouchTarget = null
  })

  return { onPointerDown, onPointerMove, onPointerUp, onPointerCancel, onClick }
}
