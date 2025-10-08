<script setup lang="ts">
import { ref, watch, computed } from 'vue'
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs'
import { Button } from '@/components/ui/button'
import { Textarea } from '@/components/ui/textarea'
import { WandSparklesIcon, RotateCcwIcon } from 'lucide-vue-next'
import ParamFormBuilder from './ParamFormBuilder.vue'
import { useMethodSignature } from '../composables/useMethodSignature'
import type { FormData } from '../types'

interface Props {
  methodName?: string | null
}

interface Emits {
  (e: 'update:params', params: unknown): void
}

const props = defineProps<Props>()
const emit = defineEmits<Emits>()

const { error, formFields, fetchSignature, generateDefaultParams } = useMethodSignature()

// 当前模式：form 或 json
const currentMode = ref<'form' | 'json'>('form')

// 表单数据
const formData = ref<FormData>({})

// JSON 输入
const jsonInput = ref('{}')

// JSON 验证
const isValidJson = computed(() => {
  try {
    JSON.parse(jsonInput.value)
    return true
  } catch {
    return false
  }
})

// 监听方法变化
watch(
  () => props.methodName,
  async (newMethod) => {
    if (!newMethod) {
      formData.value = {}
      jsonInput.value = '{}'
      return
    }

    // 获取方法签名
    await fetchSignature(newMethod)

    // 生成默认参数
    if (formFields.value.length > 0) {
      const defaultParams = generateDefaultParams()
      formData.value = defaultParams
      jsonInput.value = JSON.stringify(defaultParams, null, 2)
    } else {
      formData.value = {}
      jsonInput.value = '{}'
    }
  },
  { immediate: true }
)

// 表单数据变化时同步到 JSON
watch(
  formData,
  (newData) => {
    if (currentMode.value === 'form') {
      jsonInput.value = JSON.stringify(newData, null, 2)
      emit('update:params', newData)
    }
  },
  { deep: true }
)

// JSON 输入变化时（在 JSON 模式下）
watch(jsonInput, (newJson) => {
  if (currentMode.value === 'json' && isValidJson.value) {
    try {
      const parsed = JSON.parse(newJson)
      formData.value = parsed
      emit('update:params', parsed)
    } catch (error) {
      console.error('Failed to parse JSON:', error)
    }
  }
})

// 模式切换时同步数据
watch(currentMode, (newMode) => {
  if (newMode === 'json') {
    // 切换到 JSON 模式：从表单数据同步
    jsonInput.value = JSON.stringify(formData.value, null, 2)
  } else {
    // 切换到表单模式：从 JSON 同步
    if (isValidJson.value) {
      try {
        formData.value = JSON.parse(jsonInput.value)
      } catch (error) {
        console.error('Failed to sync to form:', error)
      }
    }
  }
})

// 格式化 JSON
const formatJson = () => {
  if (isValidJson.value) {
    try {
      const parsed = JSON.parse(jsonInput.value)
      jsonInput.value = JSON.stringify(parsed, null, 2)
    } catch {
      // 忽略错误
    }
  }
}

// 生成模板（一键填充默认值）
const generateTemplate = () => {
  const defaultParams = generateDefaultParams()
  formData.value = defaultParams
  jsonInput.value = JSON.stringify(defaultParams, null, 2)
}

// 重置参数
const resetParams = () => {
  formData.value = {}
  jsonInput.value = '{}'
  emit('update:params', {})
}

// 获取当前参数
const getCurrentParams = (): unknown => {
  if (currentMode.value === 'json' && isValidJson.value) {
    return JSON.parse(jsonInput.value)
  }
  return formData.value
}

// 暴露方法给父组件
defineExpose({
  getCurrentParams,
  isValidJson,
})
</script>

<template>
  <div class="flex h-full flex-col">
    <!-- 标题栏 -->
    <div class="flex items-center justify-between border-b p-3">
      <h4 class="text-sm font-medium">请求参数</h4>
      <div class="flex items-center gap-2">
        <!-- 生成模板按钮 -->
        <Button v-if="formFields.length > 0" variant="outline" size="sm" @click="generateTemplate">
          <WandSparklesIcon class="mr-1 h-3 w-3" />
          生成模板
        </Button>

        <!-- 重置按钮 -->
        <Button variant="outline" size="sm" @click="resetParams">
          <RotateCcwIcon class="mr-1 h-3 w-3" />
          重置
        </Button>
      </div>
    </div>

    <!-- 错误提示 -->
    <div
      v-if="error"
      class="m-4 rounded-lg border border-red-200 bg-red-50 p-3 text-sm text-red-800 dark:border-red-800 dark:bg-red-950 dark:text-red-200"
    >
      <p>{{ error }}</p>
    </div>

    <!-- 参数输入区域 -->
    <div v-else class="flex-1 overflow-y-auto p-4">
      <Tabs v-model="currentMode" class="w-full">
        <!-- 模式切换标签 -->
        <TabsList class="grid w-full grid-cols-2">
          <TabsTrigger value="form">表单模式</TabsTrigger>
          <TabsTrigger value="json">JSON 模式</TabsTrigger>
        </TabsList>

        <!-- 表单模式 -->
        <TabsContent value="form" class="mt-4">
          <ParamFormBuilder v-model="formData" :fields="formFields" />
        </TabsContent>

        <!-- JSON 模式 -->
        <TabsContent value="json" class="mt-4 space-y-3">
          <div class="flex items-center justify-between">
            <p class="text-sm text-muted-foreground">直接编辑 JSON 格式的参数</p>
            <Button variant="outline" size="sm" @click="formatJson" :disabled="!isValidJson">
              格式化
            </Button>
          </div>

          <Textarea
            v-model="jsonInput"
            placeholder='{"key": "value"}'
            class="min-h-[300px] font-mono text-sm"
            :class="!isValidJson && 'border-red-500 focus:border-red-500'"
          />

          <!-- JSON 格式错误提示 -->
          <div v-if="!isValidJson" class="flex items-center text-xs text-red-500">
            <svg class="mr-1 h-4 w-4" fill="currentColor" viewBox="0 0 20 20">
              <path
                fill-rule="evenodd"
                d="M18 10a8 8 0 11-16 0 8 8 0 0116 0zm-7 4a1 1 0 11-2 0 1 1 0 012 0zm-1-9a1 1 0 00-1 1v4a1 1 0 102 0V6a1 1 0 00-1-1z"
                clip-rule="evenodd"
              />
            </svg>
            JSON 格式错误，请检查语法
          </div>
        </TabsContent>
      </Tabs>
    </div>
  </div>
</template>
