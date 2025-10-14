<script setup lang="ts">
import { computed, ref, toRef, watch } from 'vue'
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
   * 分隔条宽度（像素）
   * @default 4
   */
  dividerSize?: number

  /**
   * 分隔条自定义类名
   */
  dividerClass?: string

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
  dividerSize: 4,
  dividerClass: '',
  pane1Class: '',
  pane2Class: '',
  reverse: false,
})

const emit = defineEmits<{
  'update:size': [size: number | string]
  'drag-start': [e: MouseEvent]
  'drag-end': [e: MouseEvent]
}>()

// 非受控状态：存储组件内部的尺寸值
const uncontrolledSize = ref(props.defaultSize)

// 内部状态管理（支持受控和非受控）
const internalSize = computed({
  get: () => props.size ?? uncontrolledSize.value,
  set: (value) => {
    emit('update:size', value)
    // 如果是非受控模式（没有外部 size prop），更新内部状态
    if (props.size === undefined) {
      uncontrolledSize.value = value
    }
  },
})

// 当 defaultSize 变化且处于非受控模式时，同步到内部状态
watch(
  () => props.defaultSize,
  (newDefault) => {
    if (props.size === undefined) {
      uncontrolledSize.value = newDefault
    }
  }
)

// 使用拖拽逻辑
const {
  containerRef,
  dividerRef,
  isDragging,
  dividerStyle,
  dividerCursor,
  handleMouseDown,
  getFirstPaneStyle,
  getSecondPaneStyle,
} = useSplitResize({
  direction: toRef(props, 'direction'),
  dividerSize: toRef(props, 'dividerSize'),
  min: toRef(props, 'min'),
  max: toRef(props, 'max'),
  reverse: toRef(props, 'reverse'),
  onUpdate: (size) => {
    internalSize.value = size
  },
  onDragStart: (e) => emit('drag-start', e),
  onDragEnd: (e) => emit('drag-end', e),
})

// 计算样式
const containerClass = computed(() => [
  'flex w-full h-full',
  props.direction === 'horizontal' ? 'flex-row' : 'flex-col',
])

const firstPaneStyle = computed(() => getFirstPaneStyle(internalSize.value))
const secondPaneStyle = computed(() => getSecondPaneStyle(internalSize.value))

const dividerClasses = computed(() => [
  'group flex-shrink-0 transition-colors duration-200',
  dividerCursor.value,
  props.dividerClass || 'bg-border hover:bg-primary/50',
  isDragging.value && 'bg-primary/70',
  props.disabled && 'cursor-default opacity-50',
])

// 拖拽处理器
const onMouseDown = (e: MouseEvent) => {
  if (props.disabled) return
  handleMouseDown(e, internalSize.value)
}
</script>

<template>
  <div ref="containerRef" :class="containerClass">
    <!-- 面板 1 -->
    <div :class="['min-h-0 min-w-0 overflow-hidden', pane1Class]" :style="firstPaneStyle">
      <slot name="1" :panel="1">
        <slot :panel="1" />
      </slot>
    </div>

    <!-- 分隔条 -->
    <div
      v-if="!disabled"
      ref="dividerRef"
      :class="dividerClasses"
      :style="dividerStyle"
      @mousedown="onMouseDown"
    >
      <slot name="divider" />
    </div>

    <!-- 面板 2 -->
    <div :class="['min-h-0 min-w-0 overflow-hidden', pane2Class]" :style="secondPaneStyle">
      <slot name="2" :panel="2">
        <slot :panel="2" />
      </slot>
    </div>
  </div>
</template>
