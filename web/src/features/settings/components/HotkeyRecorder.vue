
<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch } from 'vue'
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

// 更新显示文本
watch([() => props.value, isRecording, t], () => {
  if (!isRecording.value) {
    const text = formatHotkeyDisplay(props.value.modifiers, props.value.key)
    displayText.value = text || t('settings.general.hotkey.recorder.notSet')
  }
}, { immediate: true })

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
}

const stopRecording = () => {
  isRecording.value = false
  currentModifiers.value = 0
  currentKey.value = 0
}

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
  if (
    [
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
    ].includes(e.key)
  ) {
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
  if (
    [
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
    ].includes(e.key)
  ) {
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
  if (recorderRef.value && !recorderRef.value.contains(e.target as Node)) {
    stopRecording()
  }
}

onMounted(() => {
  window.addEventListener('keydown', handleKeyDown)
  window.addEventListener('keyup', handleKeyUp)
  document.addEventListener('mousedown', handleClickOutside)
})

onUnmounted(() => {
  window.removeEventListener('keydown', handleKeyDown)
  window.removeEventListener('keyup', handleKeyUp)
  document.removeEventListener('mousedown', handleClickOutside)
})
</script>

<template>
  <div ref="recorderRef" :class="cn('w-full', className)">
    <div
      :class="cn(
        'rounded-md border border-input bg-background px-3 py-2 text-sm',
        'focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2',
        'cursor-pointer transition-colors',
        isRecording
          ? 'border-primary ring-2 ring-primary ring-offset-2'
          : 'hover:border-accent-foreground'
      )"
      @click="startRecording"
      tabindex="0"
    >
      {{ isRecording ? (displayText || t('settings.general.hotkey.recorder.pressKey')) : displayText }}
    </div>
  </div>
</template>
