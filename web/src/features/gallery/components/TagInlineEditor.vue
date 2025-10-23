<script setup lang="ts">
import { ref, nextTick, onMounted } from 'vue'

interface Props {
  initialValue?: string
  placeholder?: string
}

const props = withDefaults(defineProps<Props>(), {
  initialValue: '',
  placeholder: '输入标签名...',
})

const emit = defineEmits<{
  confirm: [value: string]
  cancel: []
}>()

const inputValue = ref(props.initialValue)
const inputRef = ref<HTMLInputElement>()

onMounted(() => {
  nextTick(() => {
    inputRef.value?.focus()
    // 如果有初始值，选中所有文本
    if (props.initialValue) {
      inputRef.value?.select()
    }
  })
})

function handleConfirm() {
  const trimmedValue = inputValue.value.trim()
  if (trimmedValue) {
    emit('confirm', trimmedValue)
  } else {
    emit('cancel')
  }
}

function handleCancel() {
  emit('cancel')
}

function handleBlur() {
  // 延迟执行，避免与点击事件冲突
  setTimeout(() => {
    handleConfirm()
  }, 100)
}

function handleKeydown(event: KeyboardEvent) {
  if (event.key === 'Enter') {
    event.preventDefault()
    handleConfirm()
  } else if (event.key === 'Escape') {
    event.preventDefault()
    handleCancel()
  }
}
</script>

<template>
  <div class="flex items-center">
    <input
      ref="inputRef"
      v-model="inputValue"
      type="text"
      :placeholder="placeholder"
      class="h-8 w-full rounded border border-input bg-background px-2 text-sm focus:outline-none focus:ring-2 focus:ring-ring"
      @blur="handleBlur"
      @keydown="handleKeydown"
    />
  </div>
</template>
