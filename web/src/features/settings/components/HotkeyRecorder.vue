<script setup lang="ts">
import { ref, watch } from 'vue'
import { useEventListener } from '@vueuse/core'
import { cn } from '@/lib/utils'
import { formatHotkeyDisplay, calculateModifiers } from '../utils/hotkeyUtils'
import { useI18n } from '@/composables/useI18n'

interface Hotkey {
  modifiers: number
  key: number
}

const props = defineProps<{
  value: Hotkey
  className?: string
}>()

const emit = defineEmits<{
  (e: 'change', value: Hotkey): void
}>()

const { t } = useI18n()
const isRecording = ref(false)
const displayText = ref('')
const currentModifiers = ref(0)
const currentKey = ref(0)
const recorderRef = ref<HTMLDivElement | null>(null)
const suppressedMouseButton = ref<number | null>(null)
const MODIFIER_KEYS = new Set([
  'Control',
  'ControlLeft',
  'ControlRight',
  'Alt',
  'AltLeft',
  'AltRight',
  'Shift',
  'ShiftLeft',
  'ShiftRight',
  'Meta',
  'MetaLeft',
  'MetaRight',
  'OS',
  'Win',
])

// 更新显示文本
watch(
  [() => props.value, isRecording, t],
  () => {
    if (!isRecording.value) {
      const text = formatHotkeyDisplay(props.value.modifiers, props.value.key)
      displayText.value = text || t('settings.general.hotkey.recorder.notSet')
    }
  },
  { immediate: true }
)

// 实时更新录制时的显示文本
watch([currentModifiers, currentKey, isRecording, t], () => {
  if (isRecording.value) {
    const text = formatHotkeyDisplay(currentModifiers.value, currentKey.value)
    displayText.value = text || t('settings.general.hotkey.recorder.pressKey')
  }
})

const startRecording = () => {
  isRecording.value = true
  currentModifiers.value = 0
  currentKey.value = 0
  suppressedMouseButton.value = null
}

const stopRecording = () => {
  isRecording.value = false
  currentModifiers.value = 0
  currentKey.value = 0
}

const mapMouseButtonToVirtualKey = (button: number): number => {
  if (button === 3) return 0x05 // VK_XBUTTON1
  if (button === 4) return 0x06 // VK_XBUTTON2
  return 0
}

const isModifierKey = (key: string): boolean => MODIFIER_KEYS.has(key)

const handleKeyDown = (e: KeyboardEvent) => {
  if (!isRecording.value) return

  // 阻止默认行为
  e.preventDefault()
  e.stopPropagation()

  // 处理特殊按键
  if (e.key === 'Escape') {
    stopRecording()
    return
  }

  if (e.key === 'Backspace') {
    // 清除快捷键
    emit('change', { modifiers: 0, key: 0 })
    stopRecording()
    return
  }

  // 获取修饰键状态
  const modifiers = calculateModifiers(e.shiftKey, e.ctrlKey, e.altKey, e.metaKey)

  // 如果是修饰键，只更新修饰键状态并实时显示
  if (isModifierKey(e.key)) {
    currentModifiers.value = modifiers
    return
  }

  // 获取主键
  const key = e.keyCode
  currentKey.value = key
  currentModifiers.value = modifiers

  // 更新值
  emit('change', { modifiers, key })

  // 停止录制
  stopRecording()
}

const handleKeyUp = (e: KeyboardEvent) => {
  if (!isRecording.value) return

  // 当修饰键被释放时更新修饰键状态
  if (isModifierKey(e.key)) {
    const modifiers = calculateModifiers(
      e.key.startsWith('Shift') ? false : e.shiftKey || false,
      e.key.startsWith('Control') ? false : e.ctrlKey || false,
      e.key.startsWith('Alt') ? false : e.altKey || false,
      e.key.startsWith('Meta') || e.key.startsWith('OS') || e.key === 'Win'
        ? false
        : e.metaKey || false
    )
    currentModifiers.value = modifiers
  }
}

const handleClickOutside = (e: MouseEvent) => {
  if (!isRecording.value) return

  const mouseKey = mapMouseButtonToVirtualKey(e.button)
  if (mouseKey !== 0) {
    e.preventDefault()
    e.stopPropagation()

    const modifiers = calculateModifiers(e.shiftKey, e.ctrlKey, e.altKey, e.metaKey)
    currentModifiers.value = modifiers
    currentKey.value = mouseKey
    // side-button shortcuts may trigger browser history on mouseup/auxclick;
    // keep a one-shot guard for this button to suppress that navigation.
    suppressedMouseButton.value = e.button
    emit('change', { modifiers, key: mouseKey })
    stopRecording()
    return
  }

  if (recorderRef.value && !recorderRef.value.contains(e.target as Node)) {
    stopRecording()
  }
}

const handleSideButtonNavigationGuard = (e: MouseEvent) => {
  const mouseKey = mapMouseButtonToVirtualKey(e.button)
  if (mouseKey === 0) return

  const shouldSuppress = isRecording.value || suppressedMouseButton.value === e.button
  if (!shouldSuppress) return

  e.preventDefault()
  e.stopPropagation()

  if (suppressedMouseButton.value === e.button && (e.type === 'mouseup' || e.type === 'auxclick')) {
    suppressedMouseButton.value = null
  }
}

useEventListener(window, 'keydown', handleKeyDown)
useEventListener(window, 'keyup', handleKeyUp)
useEventListener(document, 'mousedown', handleClickOutside, { capture: true })
useEventListener(document, 'mouseup', handleSideButtonNavigationGuard, { capture: true })
useEventListener(document, 'auxclick', handleSideButtonNavigationGuard, { capture: true })
</script>

<template>
  <div ref="recorderRef" :class="cn('w-full', className)">
    <div
      :class="
        cn(
          'rounded-md border border-input bg-background px-3 py-2 text-sm',
          'focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2',
          'cursor-pointer transition-colors',
          isRecording
            ? 'border-primary ring-2 ring-primary ring-offset-2'
            : 'hover:border-accent-foreground'
        )
      "
      @click="startRecording"
      tabindex="0"
    >
      {{
        isRecording ? displayText || t('settings.general.hotkey.recorder.pressKey') : displayText
      }}
    </div>
  </div>
</template>
