<script setup lang="ts">
import { ref, computed } from 'vue'
import { CopyIcon, CheckIcon, ChevronDownIcon, ChevronRightIcon } from 'lucide-vue-next'
import { Button } from '@/components/ui/button'
import type { ApiTestResponse } from '../types'

interface Props {
  response?: ApiTestResponse | null
  loading?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  response: null,
  loading: false,
})

const isExpanded = ref(true)
const copySuccess = ref(false)

// 格式化的JSON字符串
const formattedJson = computed(() => {
  if (!props.response) return ''

  try {
    const data = props.response.success ? props.response.data : props.response.error
    return JSON.stringify(data, null, 2)
  } catch {
    return String(props.response.success ? props.response.data : props.response.error)
  }
})

// 复制到剪贴板
const copyToClipboard = async () => {
  try {
    await navigator.clipboard.writeText(formattedJson.value)
    copySuccess.value = true
    setTimeout(() => {
      copySuccess.value = false
    }, 2000)
  } catch (error) {
    console.error('Failed to copy to clipboard:', error)
  }
}

// 切换展开状态
const toggleExpanded = () => {
  isExpanded.value = !isExpanded.value
}

// 格式化时间
const formatTime = (timestamp: number) => {
  return new Date(timestamp).toLocaleTimeString()
}

// 格式化持续时间
const formatDuration = (duration: number) => {
  if (duration < 1000) {
    return `${duration}ms`
  }
  return `${(duration / 1000).toFixed(2)}s`
}
</script>

<template>
  <div class="overflow-hidden rounded-lg border">
    <!-- 响应头部 -->
    <div class="flex items-center justify-between border-b bg-muted/50 p-3">
      <div class="flex items-center space-x-2">
        <Button variant="ghost" size="sm" @click="toggleExpanded" class="h-6 w-6 p-1">
          <ChevronRightIcon v-if="!isExpanded" class="size-4" />
          <ChevronDownIcon v-else class="size-4" />
        </Button>

        <div class="flex items-center space-x-2">
          <div
            class="h-2 w-2 rounded-full"
            :class="response?.success ? 'bg-green-500' : 'bg-red-500'"
          />
          <span class="text-sm font-medium">
            {{ response?.success ? '成功' : '失败' }}
          </span>
        </div>

        <div v-if="response" class="text-xs text-muted-foreground">
          {{ formatTime(response.timestamp) }} · {{ formatDuration(response.duration) }}
        </div>
      </div>

      <Button
        variant="ghost"
        size="sm"
        @click="copyToClipboard"
        :disabled="!formattedJson"
        class="h-6 w-6 p-1"
      >
        <CheckIcon v-if="copySuccess" class="size-4 text-green-500" />
        <CopyIcon v-else class="size-4" />
      </Button>
    </div>

    <!-- 响应内容 -->
    <div v-if="isExpanded" class="relative">
      <!-- 加载状态 -->
      <div v-if="loading" class="flex items-center justify-center p-8">
        <div
          class="size-6 animate-spin rounded-full border-2 border-current border-t-transparent"
        />
        <span class="ml-2 text-sm text-muted-foreground">执行中...</span>
      </div>

      <!-- 无响应 -->
      <div v-else-if="!response" class="flex items-center justify-center p-8">
        <div class="text-sm text-muted-foreground">暂无响应数据</div>
      </div>

      <!-- 响应内容 -->
      <div v-else class="relative">
        <pre
          class="overflow-x-auto rounded-md border-0 bg-slate-50 p-4 text-sm dark:bg-slate-900"
          :class="response?.success ? 'border-green-200' : 'border-red-200'"
        ><code>{{ formattedJson }}</code></pre>
      </div>
    </div>
  </div>
</template>

<style scoped>
pre {
  margin: 0;
  font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
  white-space: pre;
  word-wrap: normal;
}

code {
  color: inherit;
}
</style>
