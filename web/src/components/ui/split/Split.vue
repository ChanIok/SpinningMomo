<script setup lang="ts">
import { computed, ref, toRef, useSlots, watch } from 'vue'
import { useSplitResize } from './useSplitResize'

export interface SplitProps {
  /**
   * 分割方向
   * @default 'horizontal'
   */
  direction?: 'horizontal' | 'vertical'

  /**
   * 默认大小（非受控）
   * - 数字：百分比 0-1
   * - 字符串：像素值 "200px"
   * @default 0.5
   */
  defaultSize?: number | string

  /**
   * 当前大小（受控，配合 v-model:size）
   */
  size?: number | string

  /**
   * 最小尺寸
   * - 数字：百分比 0-1
   * - 字符串：像素值 "100px"
   * @default 0
   */
  min?: number | string

  /**
   * 最大尺寸
   * - 数字：百分比 0-1
   * - 字符串：像素值 "500px"
   * @default 1
   */
  max?: number | string

  /**
   * 禁用拖拽
   * @default false
   */
  disabled?: boolean

  /**
   * 分隔条命中区域宽度（像素）
   * - 覆盖式，不占布局；仅影响拖拽命中区
   * @default 4
   */
  dividerSize?: number

  /**
   * 分隔条视觉线宽度（像素）
   * - 仅影响可见线条粗细，不影响命中区域
   * @default 1
   */
  dividerVisualSize?: number

  /**
   * 分隔条拖拽区域自定义类名
   */
  dividerClass?: string

  /**
   * 分隔条视觉线自定义类名
   */
  dividerLineClass?: string

  /**
   * 面板 1 自定义类名
   */
  pane1Class?: string

  /**
   * 面板 2 自定义类名
   */
  pane2Class?: string

  /**
   * 反向模式：控制第二个面板的尺寸而非第一个
   * - false（默认）：尺寸参数控制 template #1，template #2 自适应
   * - true：尺寸参数控制 template #2，template #1 自适应
   * @default false
   */
  reverse?: boolean
}

const props = withDefaults(defineProps<SplitProps>(), {
  direction: 'horizontal',
  defaultSize: 0.5,
  min: 0,
  max: 1,
  disabled: false,
  dividerSize: 5,
  dividerVisualSize: 1,
  dividerClass: '',
  dividerLineClass: '',
  pane1Class: '',
  pane2Class: '',
  reverse: false,
})

const emit = defineEmits<{
  'update:size': [size: number | string]
  drag: [e: MouseEvent]
  'drag-start': [e: MouseEvent]
  'drag-end': [e: MouseEvent]
}>()

// 同时支持受控和非受控两种用法：
// 传入 size 时由外部驱动，否则回退到组件内部状态。
const uncontrolledSize = ref(props.defaultSize)

const internalSize = computed({
  get: () => props.size ?? uncontrolledSize.value,
  set: (value) => {
    emit('update:size', value)
    if (props.size === undefined) {
      uncontrolledSize.value = value
    }
  },
})

watch(
  () => props.defaultSize,
  (newDefault) => {
    if (props.size === undefined) {
      uncontrolledSize.value = newDefault
    }
  }
)

const splitResize = useSplitResize({
  direction: toRef(props, 'direction'),
  dividerSize: toRef(props, 'dividerSize'),
  min: toRef(props, 'min'),
  max: toRef(props, 'max'),
  reverse: toRef(props, 'reverse'),
  onUpdate: (size) => {
    internalSize.value = size
  },
  onDrag: (e) => emit('drag', e),
  onDragStart: (e) => emit('drag-start', e),
  onDragEnd: (e) => emit('drag-end', e),
})

const { isDragging, dividerStyle, dividerCursor, getFirstPaneStyle, getSecondPaneStyle } =
  splitResize

const containerClass = computed(() => [
  'flex h-full w-full',
  props.direction === 'horizontal' ? 'flex-row' : 'flex-col',
])

const firstPaneStyle = computed(() => getFirstPaneStyle(internalSize.value))
const secondPaneStyle = computed(() => getSecondPaneStyle(internalSize.value))
const slots = useSlots()

const dividerClasses = computed(() => [
  'group absolute z-999 flex-shrink-0',
  props.direction === 'horizontal'
    ? 'top-0 left-1/2 h-full -translate-x-1/2'
    : 'top-1/2 left-0 w-full -translate-y-1/2',
  dividerCursor.value,
  props.dividerClass || 'bg-transparent',
  props.disabled && 'cursor-default opacity-50',
])

const dividerLineClasses = computed(() => [
  // dividerSize 决定拖拽命中区，dividerVisualSize 只控制可见细线。
  'pointer-events-none absolute transition-colors duration-200',
  props.direction === 'horizontal'
    ? 'top-0 left-1/2 h-full -translate-x-1/2'
    : 'top-1/2 left-0 w-full -translate-y-1/2',
  props.dividerLineClass ||
    (isDragging.value ? 'bg-primary' : 'bg-border group-hover:bg-primary/50'),
])

const dividerLineStyle = computed(() => {
  const isHorizontal = props.direction === 'horizontal'
  return isHorizontal
    ? { width: `${props.dividerVisualSize}px` }
    : { height: `${props.dividerVisualSize}px` }
})

const dividerAnchorClass = computed(() =>
  props.direction === 'horizontal'
    ? 'relative h-full w-0 flex-shrink-0 overflow-visible'
    : 'relative h-0 w-full flex-shrink-0 overflow-visible'
)

const hasDividerSlot = computed(() => Boolean(slots.divider))

const onMouseDown = (e: MouseEvent) => {
  if (props.disabled) return
  splitResize.handleMouseDown(e, internalSize.value)
}
</script>

<template>
  <div :ref="splitResize.containerRef" :class="['relative', containerClass]">
    <div :class="['min-h-0 min-w-0 overflow-hidden', pane1Class]" :style="firstPaneStyle">
      <slot name="1" :panel="1">
        <slot :panel="1" />
      </slot>
    </div>

    <div v-if="!disabled" :class="dividerAnchorClass">
      <div
        :ref="splitResize.dividerRef"
        :class="dividerClasses"
        :style="dividerStyle"
        @mousedown="onMouseDown"
      >
        <div v-if="!hasDividerSlot" :class="dividerLineClasses" :style="dividerLineStyle" />
        <slot name="divider" />
      </div>
    </div>

    <div :class="['min-h-0 min-w-0 overflow-hidden', pane2Class]" :style="secondPaneStyle">
      <slot name="2" :panel="2">
        <slot :panel="2" />
      </slot>
    </div>
  </div>
</template>
