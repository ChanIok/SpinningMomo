<script setup lang="ts">
import { ref, nextTick, onMounted } from 'vue'
import { Folder } from 'lucide-vue-next'

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
    <svg
      v-else-if="iconType === 'tag'"
      xmlns="http://www.w3.org/2000/svg"
      width="14"
      height="14"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      stroke-width="2"
      stroke-linecap="round"
      stroke-linejoin="round"
      class="flex-shrink-0 text-sidebar-foreground/70"
    >
      <path d="M12 2H2v10l9.29 9.29c.94.94 2.48.94 3.42 0l6.58-6.58c.94-.94.94-2.48 0-3.42L12 2Z" />
      <path d="M7 7h.01" />
    </svg>

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
