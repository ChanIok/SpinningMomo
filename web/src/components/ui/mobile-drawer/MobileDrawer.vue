<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch, type HTMLAttributes } from 'vue'
import { cn } from '@/lib/utils'

/** 触发拖拽关闭的最小位移阈值（像素） */
const DISMISS_DISTANCE_THRESHOLD = 70

/** 触发轻扫快速甩出关闭的速度阈值（像素/毫秒） */
const DISMISS_VELOCITY_THRESHOLD = 0.4

/** 判定手势主方向锁定的最小移动距离（像素） */
const DIRECTION_LOCK_THRESHOLD = 10

/** 遮罩完全淡出所需的拖拽参考距离（像素） */
const BACKDROP_FADE_DISTANCE = 300

/** 未达阈值时阻尼弹簧回弹的动画时长（毫秒） */
const SNAPBACK_DURATION_MS = 220

/** 离开动画播放完毕后重置拖拽状态的安全延迟（毫秒） */
const LEAVE_CLEANUP_DELAY_MS = 200

interface MobileDrawerProps {
  open: boolean
  side?: 'bottom' | 'right' | 'left' | 'top'
  class?: HTMLAttributes['class']
  /** Teleport 后的层级；沉浸式暗房需要覆盖自身的 z-index。 */
  zIndex?: number
  /** 是否由抽屉自身消费 Escape；需要由外层历史协调器处理时关闭。 */
  closeOnEscape?: boolean
  showHandle?: boolean
  dismissible?: boolean
}

const props = withDefaults(defineProps<MobileDrawerProps>(), {
  side: 'bottom',
  zIndex: 50,
  closeOnEscape: true,
  showHandle: true,
  dismissible: true,
})

const emit = defineEmits<{
  close: []
  'update:open': [value: boolean]
}>()

const panelRef = ref<HTMLElement | null>(null)
const isDragging = ref(false)
const isDismissingByDrag = ref(false)
const isSnappingBack = ref(false)
const dragOffsetX = ref(0)
const dragOffsetY = ref(0)

let touchStartX = 0
let touchStartY = 0
let touchStartTime = 0
let isEligibleDrag = false
let gestureDirection: 'horizontal' | 'vertical' | 'undecided' = 'undecided'

function handleClose() {
  emit('close')
  emit('update:open', false)
}

function handleKeydown(event: KeyboardEvent) {
  if (event.key === 'Escape' && props.open && props.closeOnEscape) {
    event.stopPropagation()
    handleClose()
  }
}

function getScrollParent(node: HTMLElement | null): HTMLElement | null {
  let current = node
  while (current && current !== panelRef.value) {
    const overflowY = window.getComputedStyle(current).overflowY
    if (overflowY === 'auto' || overflowY === 'scroll') {
      return current
    }
    current = current.parentElement
  }
  return null
}

function isInteractiveTarget(target: HTMLElement | null): boolean {
  if (!target) return false
  return Boolean(
    target.closest(
      'button, a, input, textarea, select, [role="button"], [role="slider"], [data-no-drawer-drag="true"], .reka-slider-root, .color-picker-spectrum'
    )
  )
}

function handleTouchStart(event: TouchEvent) {
  if (!props.dismissible || event.touches.length !== 1) {
    return
  }

  const touch = event.touches[0]
  touchStartX = touch.clientX
  touchStartY = touch.clientY
  touchStartTime = performance.now()
  dragOffsetX.value = 0
  dragOffsetY.value = 0
  gestureDirection = 'undecided'
  isDismissingByDrag.value = false
  isSnappingBack.value = false

  const target = event.target as HTMLElement | null

  // 避让 Slider、Input、取色盘等交互控件
  if (isInteractiveTarget(target)) {
    isEligibleDrag = false
    return
  }

  const isHandle = Boolean(target?.closest('[data-drawer-handle="true"]'))
  if (isHandle) {
    isEligibleDrag = true
    return
  }

  const scrollContainer = getScrollParent(target)
  if (props.side === 'bottom') {
    isEligibleDrag = !scrollContainer || scrollContainer.scrollTop <= 0
  } else {
    isEligibleDrag = true
  }
}

function handleTouchMove(event: TouchEvent) {
  if (!isEligibleDrag || event.touches.length !== 1) {
    return
  }

  const touch = event.touches[0]
  const deltaX = touch.clientX - touchStartX
  const deltaY = touch.clientY - touchStartY
  const absX = Math.abs(deltaX)
  const absY = Math.abs(deltaY)

  // 1. 方向锁判定（超过阈值后单向锁定，防止斜向滚动误触）
  if (gestureDirection === 'undecided') {
    if (absX > DIRECTION_LOCK_THRESHOLD || absY > DIRECTION_LOCK_THRESHOLD) {
      if (props.side === 'bottom' || props.side === 'top') {
        if (absY > absX) {
          gestureDirection = 'vertical'
        } else {
          isEligibleDrag = false
          return
        }
      } else if (props.side === 'left' || props.side === 'right') {
        if (absX > absY) {
          gestureDirection = 'horizontal'
        } else {
          // 用户在左右抽屉中上下滚动长列表 → 锁定放行垂直滚动，禁用抽屉拖拽
          isEligibleDrag = false
          return
        }
      }
    } else {
      return
    }
  }

  // 2. 根据抽屉方向追踪位移
  if (props.side === 'bottom') {
    if (deltaY > 0) {
      isDragging.value = true
      dragOffsetY.value = deltaY
    } else {
      dragOffsetY.value = 0
      isDragging.value = false
    }
  } else if (props.side === 'right') {
    // 右侧抽屉：向右推回（deltaX > 0）
    if (deltaX > 0) {
      isDragging.value = true
      dragOffsetX.value = deltaX
    } else {
      dragOffsetX.value = 0
      isDragging.value = false
    }
  } else if (props.side === 'left') {
    // 左侧抽屉：向左推回（deltaX < 0）
    if (deltaX < 0) {
      isDragging.value = true
      dragOffsetX.value = deltaX
    } else {
      dragOffsetX.value = 0
      isDragging.value = false
    }
  }
}

function handleTouchEnd() {
  if (!isDragging.value) {
    isEligibleDrag = false
    gestureDirection = 'undecided'
    dragOffsetX.value = 0
    dragOffsetY.value = 0
    return
  }

  const duration = performance.now() - touchStartTime
  isEligibleDrag = false
  gestureDirection = 'undecided'

  let shouldDismiss = false

  if (props.side === 'bottom') {
    const deltaY = dragOffsetY.value
    const velocity = deltaY / Math.max(duration, 1)
    shouldDismiss = deltaY > DISMISS_DISTANCE_THRESHOLD || velocity > DISMISS_VELOCITY_THRESHOLD
  } else if (props.side === 'right') {
    const deltaX = dragOffsetX.value
    const velocity = deltaX / Math.max(duration, 1)
    shouldDismiss = deltaX > DISMISS_DISTANCE_THRESHOLD || velocity > DISMISS_VELOCITY_THRESHOLD
  } else if (props.side === 'left') {
    const deltaX = dragOffsetX.value
    const velocity = Math.abs(deltaX) / Math.max(duration, 1)
    shouldDismiss = deltaX < -DISMISS_DISTANCE_THRESHOLD || velocity > DISMISS_VELOCITY_THRESHOLD
  }

  if (shouldDismiss) {
    isDismissingByDrag.value = true
    isDragging.value = false
    isSnappingBack.value = false
    handleClose()
  } else {
    // 激活柔和阻尼弹性回弹（Spring Snapback），绝不生硬瞬切
    isDragging.value = false
    isSnappingBack.value = true
    setTimeout(() => {
      isSnappingBack.value = false
      dragOffsetX.value = 0
      dragOffsetY.value = 0
    }, SNAPBACK_DURATION_MS)
  }
}

watch(
  () => [props.open, props.closeOnEscape] as const,
  ([isOpen, closeOnEscape]) => {
    if (typeof window === 'undefined') {
      return
    }
    if (isOpen) {
      if (closeOnEscape) {
        window.addEventListener('keydown', handleKeydown)
      } else {
        window.removeEventListener('keydown', handleKeydown)
      }
      isDismissingByDrag.value = false
      isSnappingBack.value = false
      isDragging.value = false
      dragOffsetX.value = 0
      dragOffsetY.value = 0
      gestureDirection = 'undecided'
    } else {
      window.removeEventListener('keydown', handleKeydown)
      setTimeout(() => {
        isDismissingByDrag.value = false
        isSnappingBack.value = false
        isDragging.value = false
        dragOffsetX.value = 0
        dragOffsetY.value = 0
        gestureDirection = 'undecided'
      }, LEAVE_CLEANUP_DELAY_MS)
    }
  },
  { immediate: true }
)

onBeforeUnmount(() => {
  if (typeof window !== 'undefined') {
    window.removeEventListener('keydown', handleKeydown)
  }
})

const transitionName = computed(() => `mobile-drawer-${props.side}`)

const containerPlacementClass = computed(() => {
  switch (props.side) {
    case 'right':
      return 'items-stretch justify-end'
    case 'left':
      return 'items-stretch justify-start'
    case 'top':
      return 'items-start justify-center'
    case 'bottom':
    default:
      return 'items-end justify-center'
  }
})

const defaultPanelClass = computed(() => {
  switch (props.side) {
    case 'right':
      return 'h-full w-full sm:max-w-md border-l border-border'
    case 'left':
      return 'h-full w-full sm:max-w-md border-r border-border'
    case 'top':
      return 'w-full border-b border-border'
    case 'bottom':
    default:
      return 'w-full max-h-[88vh] rounded-t-2xl border-t border-border/80'
  }
})

const panelDynamicStyle = computed(() => {
  if (isDismissingByDrag.value) {
    let targetTransform = 'translate3d(0, 100%, 0)'
    if (props.side === 'right') targetTransform = 'translate3d(100%, 0, 0)'
    else if (props.side === 'left') targetTransform = 'translate3d(-100%, 0, 0)'

    return {
      transform: targetTransform,
      transition: 'transform 180ms cubic-bezier(0.4, 0, 1, 1)',
    }
  }

  if (isSnappingBack.value) {
    return {
      transform: 'translate3d(0, 0, 0)',
      transition: `transform ${SNAPBACK_DURATION_MS}ms cubic-bezier(0.16, 1, 0.3, 1)`,
    }
  }

  if (isDragging.value) {
    if (props.side === 'bottom' && dragOffsetY.value > 0) {
      return {
        transform: `translate3d(0, ${dragOffsetY.value}px, 0)`,
        transition: 'none',
      }
    }
    if ((props.side === 'right' || props.side === 'left') && dragOffsetX.value !== 0) {
      return {
        transform: `translate3d(${dragOffsetX.value}px, 0, 0)`,
        transition: 'none',
      }
    }
  }

  return undefined
})

const overlayDynamicStyle = computed(() => {
  if (isDismissingByDrag.value) {
    return {
      opacity: '0',
      transition: 'opacity 120ms cubic-bezier(0.4, 0, 1, 1)',
    }
  }

  if (isSnappingBack.value) {
    return {
      opacity: '1',
      transition: `opacity ${SNAPBACK_DURATION_MS}ms cubic-bezier(0.16, 1, 0.3, 1)`,
    }
  }

  if (isDragging.value) {
    const offset = props.side === 'bottom' ? dragOffsetY.value : Math.abs(dragOffsetX.value)
    if (offset > 0) {
      const opacity = Math.max(0, 1 - offset / BACKDROP_FADE_DISTANCE)
      return {
        opacity: String(opacity),
        transition: 'none',
      }
    }
  }
  return undefined
})
</script>

<template>
  <Teleport to="body">
    <Transition :name="transitionName" :duration="{ enter: 250, leave: 180 }">
      <div
        v-if="open"
        class="pointer-events-auto fixed inset-0 z-50 flex"
        :class="containerPlacementClass"
        :style="{ zIndex: props.zIndex }"
        role="dialog"
        aria-modal="true"
      >
        <!-- 遮罩层：点击通知外部消费历史/关闭 -->
        <div
          class="mobile-drawer-overlay absolute inset-0 cursor-default bg-black/60"
          :style="overlayDynamicStyle"
          aria-hidden="true"
          @click="handleClose"
        />

        <!-- 抽屉面板容器 -->
        <div
          ref="panelRef"
          class="mobile-drawer-panel relative z-10 flex flex-col bg-background shadow-2xl"
          :class="cn(defaultPanelClass, props.class)"
          :style="panelDynamicStyle"
          @touchstart="handleTouchStart"
          @touchmove="handleTouchMove"
          @touchend="handleTouchEnd"
          @touchcancel="handleTouchEnd"
        >
          <!-- 底部抽屉顶部居中手柄指示器 -->
          <div
            v-if="props.side === 'bottom' && props.showHandle"
            data-drawer-handle="true"
            class="flex w-full cursor-grab justify-center pt-2.5 pb-1 select-none active:cursor-grabbing"
            aria-hidden="true"
          >
            <div class="h-1 w-9 rounded-full bg-foreground/20 dark:bg-foreground/25" />
          </div>

          <slot />
        </div>
      </div>
    </Transition>
  </Teleport>
</template>

<style scoped>
/* 遮罩层：非对称淡入淡出（进入 160ms 快速变暗，离开 120ms 瞬间消退） */
[class*='mobile-drawer-'][class*='-enter-active'] .mobile-drawer-overlay {
  transition: opacity 160ms cubic-bezier(0, 0, 0.2, 1);
}

[class*='mobile-drawer-'][class*='-leave-active'] .mobile-drawer-overlay {
  transition: opacity 120ms cubic-bezier(0.4, 0, 1, 1);
}

[class*='mobile-drawer-'][class*='-enter-from'] .mobile-drawer-overlay,
[class*='mobile-drawer-'][class*='-leave-to'] .mobile-drawer-overlay {
  opacity: 0;
}

/* 抽屉面板通用进入/离开曲线（进入 250ms 陡峭减速，离开 180ms 加速撤离） */
[class*='mobile-drawer-'][class*='-enter-active'] .mobile-drawer-panel {
  transition: transform 250ms cubic-bezier(0.16, 1, 0.3, 1);
  will-change: transform;
}

[class*='mobile-drawer-'][class*='-leave-active'] .mobile-drawer-panel {
  transition: transform 180ms cubic-bezier(0.4, 0, 1, 1);
  will-change: transform;
}

/* 底部抽屉位移 */
.mobile-drawer-bottom-enter-from .mobile-drawer-panel,
.mobile-drawer-bottom-leave-to .mobile-drawer-panel {
  transform: translate3d(0, 100%, 0);
}

/* 右侧抽屉位移 */
.mobile-drawer-right-enter-from .mobile-drawer-panel,
.mobile-drawer-right-leave-to .mobile-drawer-panel {
  transform: translate3d(100%, 0, 0);
}

/* 左侧抽屉位移 */
.mobile-drawer-left-enter-from .mobile-drawer-panel,
.mobile-drawer-left-leave-to .mobile-drawer-panel {
  transform: translate3d(-100%, 0, 0);
}

/* 顶部抽屉位移 */
.mobile-drawer-top-enter-from .mobile-drawer-panel,
.mobile-drawer-top-leave-to .mobile-drawer-panel {
  transform: translate3d(0, -100%, 0);
}
</style>
