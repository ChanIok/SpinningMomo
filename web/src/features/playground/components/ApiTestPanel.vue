<script setup lang="ts">
import { ref, watch, computed, onMounted, onUnmounted } from 'vue'
import { PlayIcon } from 'lucide-vue-next'
import { Button } from '@/components/ui/button'
import ParamInputPanel from './ParamInputPanel.vue'
import JsonResponseViewer from './JsonResponseViewer.vue'
import { useApiTest } from '../composables/useApiTest'
import type { ApiMethod, ApiTestResponse } from '../types'

interface Props {
  method?: ApiMethod | null
}

interface Emits {
  (e: 'response', response: ApiTestResponse): void
}

const props = defineProps<Props>()
const emit = defineEmits<Emits>()

const { testApi, loading } = useApiTest()

const paramInputPanelRef = ref<InstanceType<typeof ParamInputPanel> | null>(null)
const currentParams = ref<unknown>({})
const lastResponse = ref<ApiTestResponse | null>(null)

// 监听方法变化，重置响应
watch(
  () => props.method,
  () => {
    lastResponse.value = null
  },
  { immediate: true }
)

// 添加键盘快捷键支持
const handleKeyDown = (event: KeyboardEvent) => {
  if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {
    event.preventDefault()
    executeTest()
  }
}

// 在组件挂载时添加键盘事件监听
onMounted(() => {
  document.addEventListener('keydown', handleKeyDown)
})

// 在组件卸载时移除键盘事件监听
onUnmounted(() => {
  document.removeEventListener('keydown', handleKeyDown)
})

// 执行API测试
const executeTest = async () => {
  if (!props.method) return

  // 检查 JSON 格式（如果在 JSON 模式下）
  if (paramInputPanelRef.value && !paramInputPanelRef.value.isValidJson) {
    console.error('Invalid JSON format')
    return
  }

  try {
    // 从 ParamInputPanel 获取当前参数
    const params = paramInputPanelRef.value?.getCurrentParams() ?? currentParams.value
    
    const response = await testApi({
      method: props.method.name,
      params,
    })

    lastResponse.value = response
    emit('response', response)
  } catch (error) {
    console.error('API test failed:', error)
  }
}

// 处理参数更新
const handleParamsUpdate = (params: unknown) => {
  currentParams.value = params
}

// 检查是否可以执行测试
const canExecuteTest = computed(() => {
  if (!props.method || loading.value) return false
  if (!paramInputPanelRef.value) return false
  return paramInputPanelRef.value.isValidJson
})
</script>

<template>
  <div class="flex h-full flex-col">
    <!-- 方法信息 -->
    <div v-if="method" class="border-b p-4">
      <h3 class="mb-2 text-lg font-semibold">{{ method.name }}</h3>
      <p v-if="method.description" class="mb-2 text-sm text-muted-foreground">
        {{ method.description }}
      </p>
      <div class="flex items-center space-x-2">
        <span
          class="inline-flex items-center rounded-full bg-blue-100 px-2 py-1 text-xs font-medium text-blue-800 dark:bg-blue-900 dark:text-blue-200"
        >
          {{ method.category || '未分类' }}
        </span>
      </div>
    </div>

    <!-- 无选中方法 -->
    <div v-else class="flex h-32 items-center justify-center text-muted-foreground">
      <div class="text-center">
        <div class="text-sm">请从左侧选择一个API方法</div>
      </div>
    </div>

    <!-- 测试面板 -->
    <div v-if="method" class="flex flex-1 flex-col min-h-0">
      <!-- 参数输入面板 -->
      <div class="flex-1 border-b">
        <ParamInputPanel
          ref="paramInputPanelRef"
          :method-name="method.name"
          @update:params="handleParamsUpdate"
        />
      </div>

      <!-- 执行按钮 -->
      <div class="border-b p-4">
        <div class="text-xs text-muted-foreground mb-2">提示：按 Ctrl+Enter 快速执行测试</div>
        <Button @click="executeTest" :disabled="!canExecuteTest" class="w-full">
          <PlayIcon v-if="!loading" class="mr-2 h-4 w-4" />
          <div
            v-else
            class="mr-2 h-4 w-4 animate-spin rounded-full border-2 border-current border-t-transparent"
          />
          {{ loading ? '执行中...' : '执行测试' }}
        </Button>
      </div>

      <!-- 响应结果 -->
      <div class="flex-1 overflow-y-auto p-4">
        <h4 class="mb-3 text-sm font-medium">响应结果</h4>
        <JsonResponseViewer :response="lastResponse" :loading="loading" />
      </div>
    </div>
  </div>
</template>
