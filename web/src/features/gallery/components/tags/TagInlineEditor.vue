<script setup lang="ts">
import { ref, nextTick, onMounted } from 'vue'
import { Folder, Tag } from '@lucide/vue'

interface Props {
  initialValue?: string
  placeholder?: string
  depth?: number
  iconType?: 'tag' | 'folder' | 'none'
}

const props = withDefaults(defineProps<Props>(), {
  initialValue: '',
  placeholder: '',
  depth: 0,
  iconType: 'tag',
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
  <div
    class="flex h-8 w-full items-center gap-2 rounded-md border border-sidebar-border bg-sidebar-accent/40 text-sidebar-foreground transition-all focus-within:border-primary/60 focus-within:bg-sidebar-accent/70 focus-within:ring-1 focus-within:ring-primary/40"
    :style="{ paddingLeft: `${depth * 12 + 7}px`, paddingRight: '7px' }"
  >
    <!-- 文件夹图标 -->
    <Folder
      v-if="iconType === 'folder'"
      class="h-3.5 w-3.5 flex-shrink-0 text-sidebar-foreground/70"
    />
    <!-- 标签图标（与 TagTreeItem 保持一致） -->
    <Tag
      v-else-if="iconType === 'tag'"
      class="h-3.5 w-3.5 flex-shrink-0 text-sidebar-foreground/70"
    />

    <!-- 输入框 -->
    <input
      ref="inputRef"
      v-model="inputValue"
      type="text"
      :placeholder="placeholder"
      class="h-full w-full border-0 bg-transparent text-sm text-sidebar-foreground outline-none placeholder:text-muted-foreground/70"
      @blur="handleBlur"
      @keydown="handleKeydown"
    />
  </div>
</template>
