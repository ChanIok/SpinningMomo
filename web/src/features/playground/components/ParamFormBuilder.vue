<script setup lang="ts">
import { computed, watch } from 'vue'
import ParamFormField from './ParamFormField.vue'
import { Separator } from '@/components/ui/separator'
import type { FormField, FormData } from '../types'

interface Props {
  fields: FormField[]
  modelValue: FormData
}

interface Emits {
  (e: 'update:modelValue', value: FormData): void
}

const props = defineProps<Props>()
const emit = defineEmits<Emits>()

// 表单数据
const formData = computed({
  get: () => props.modelValue,
  set: (val) => emit('update:modelValue', val),
})

// 更新单个字段的值
const updateFieldValue = (fieldName: string, value: unknown) => {
  formData.value = {
    ...formData.value,
    [fieldName]: value,
  }
}

// 根据类型获取默认值
const getDefaultValueForType = (type: string): unknown => {
  switch (type) {
    case 'string':
      return ''
    case 'number':
    case 'integer':
      return 0
    case 'boolean':
      return false
    case 'array':
      return []
    case 'object':
      return {}
    default:
      return null
  }
}

// 初始化表单数据
watch(
  () => props.fields,
  (newFields) => {
    if (!newFields || newFields.length === 0) return

    const initialData: FormData = { ...props.modelValue }
    let hasChanges = false

    // 为所有字段设置初始值
    newFields.forEach((field) => {
      if (!(field.name in initialData)) {
        if (field.defaultValue !== undefined) {
          initialData[field.name] = field.defaultValue
          hasChanges = true
        } else if (field.required) {
          // 为必填字段设置默认值
          initialData[field.name] = getDefaultValueForType(field.type)
          hasChanges = true
        }
      }
    })

    if (hasChanges) {
      emit('update:modelValue', initialData)
    }
  },
  { immediate: true }
)

// 是否有复杂类型（需要 JSON 编辑器）
const hasComplexTypes = computed(() => {
  return props.fields.some((field) => field.type === 'array' || field.type === 'object')
})
</script>

<template>
  <div class="space-y-4">
    <!-- 无字段提示 -->
    <div
      v-if="!fields || fields.length === 0"
      class="rounded-lg border border-dashed p-6 text-center"
    >
      <p class="text-sm text-muted-foreground">此方法无需参数，或参数 schema 为空</p>
    </div>

    <!-- 表单字段列表 -->
    <template v-else>
      <!-- 复杂类型提示 -->
      <div
        v-if="hasComplexTypes"
        class="rounded-lg border border-yellow-200 bg-yellow-50 p-3 text-sm text-yellow-800 dark:border-yellow-800 dark:bg-yellow-950 dark:text-yellow-200"
      >
        <div class="flex items-start gap-2">
          <svg
            class="mt-0.5 h-4 w-4 flex-shrink-0"
            fill="currentColor"
            viewBox="0 0 20 20"
            xmlns="http://www.w3.org/2000/svg"
          >
            <path
              fill-rule="evenodd"
              d="M8.257 3.099c.765-1.36 2.722-1.36 3.486 0l5.58 9.92c.75 1.334-.213 2.98-1.742 2.98H4.42c-1.53 0-2.493-1.646-1.743-2.98l5.58-9.92zM11 13a1 1 0 11-2 0 1 1 0 012 0zm-1-8a1 1 0 00-1 1v3a1 1 0 002 0V6a1 1 0 00-1-1z"
              clip-rule="evenodd"
            />
          </svg>
          <p>
            此方法包含复杂类型参数（数组/对象），建议切换到
            <strong>JSON 模式</strong> 进行编辑以获得更好的体验。
          </p>
        </div>
      </div>

      <!-- 渲染所有字段 -->
      <div v-for="(field, index) in fields" :key="field.name" class="space-y-4">
        <ParamFormField
          :field="field"
          :model-value="formData[field.name]"
          @update:model-value="(value) => updateFieldValue(field.name, value)"
        />

        <!-- 分隔线（最后一个字段不显示） -->
        <Separator v-if="index < fields.length - 1" class="my-4" />
      </div>
    </template>
  </div>
</template>
