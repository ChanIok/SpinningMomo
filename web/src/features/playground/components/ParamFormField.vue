<script setup lang="ts">
import { computed } from 'vue'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Checkbox } from '@/components/ui/checkbox'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { Badge } from '@/components/ui/badge'
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from '@/components/ui/tooltip'
import { InfoIcon } from 'lucide-vue-next'
import type { FormField } from '../types'

interface Props {
  field: FormField
  modelValue: unknown
}

interface Emits {
  (e: 'update:modelValue', value: unknown): void
}

const props = defineProps<Props>()
const emit = defineEmits<Emits>()

// 计算值的双向绑定
const value = computed({
  get: () => {
    const defaultValue = getDefaultValue()
    const currentValue = props.modelValue ?? defaultValue

    // 对于字符串类型，确保始终返回字符串
    if (props.field.type === 'string') {
      return String(currentValue ?? '')
    }

    // 对于数字类型，确保是数字或空字符串（供input处理）
    if (props.field.type === 'number' || props.field.type === 'integer') {
      if (typeof currentValue === 'number') {
        return currentValue
      }
      return '' // 返回空字符串让input组件处理
    }

    return currentValue
  },
  set: (val) => emit('update:modelValue', val),
})

// 获取默认值
const getDefaultValue = () => {
  if (props.field.defaultValue !== undefined) {
    return props.field.defaultValue
  }

  switch (props.field.type) {
    case 'string':
      return ''
    case 'number':
    case 'integer':
      return 0
    case 'boolean':
      return false
    default:
      return null
  }
}

// 处理数字输入
const handleNumberInput = (event: Event) => {
  const target = event.target as HTMLInputElement
  const num = props.field.type === 'integer' ? parseInt(target.value, 10) : parseFloat(target.value)
  value.value = isNaN(num) ? 0 : num
}

// 处理 checkbox 变化
const handleCheckboxChange = (checked: boolean) => {
  value.value = checked
}
</script>

<template>
  <div class="space-y-2">
    <!-- 字段标签 -->
    <div class="flex items-center gap-2">
      <Label :for="field.name" class="text-sm font-medium">
        {{ field.label }}
        <Badge v-if="field.required" variant="destructive" class="ml-2 text-xs">必填</Badge>
        <Badge v-else variant="secondary" class="ml-2 text-xs">可选</Badge>
      </Label>

      <!-- 字段描述提示 -->
      <TooltipProvider v-if="field.description" :delay-duration="300">
        <Tooltip>
          <TooltipTrigger as-child>
            <InfoIcon class="h-4 w-4 cursor-help text-muted-foreground" />
          </TooltipTrigger>
          <TooltipContent class="max-w-xs">
            <p class="text-sm">{{ field.description }}</p>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>
    </div>

    <!-- 根据类型渲染不同的输入组件 -->

    <!-- 字符串类型 - 有枚举值时使用下拉选择 -->
    <Select v-if="field.type === 'string' && field.enum" v-model="value">
      <SelectTrigger :id="field.name">
        <SelectValue :placeholder="`选择 ${field.label}`" />
      </SelectTrigger>
      <SelectContent>
        <SelectItem v-for="option in field.enum" :key="String(option)" :value="String(option)">
          {{ option }}
        </SelectItem>
      </SelectContent>
    </Select>

    <!-- 字符串类型 - 普通文本输入 -->
    <Input
      v-else-if="field.type === 'string'"
      :id="field.name"
      :model-value="value as string"
      type="text"
      :placeholder="field.defaultValue ? String(field.defaultValue) : `输入 ${field.label}`"
      :minlength="field.minLength"
      :maxlength="field.maxLength"
      :pattern="field.pattern"
      @input="(e: Event) => emit('update:modelValue', (e.target as HTMLInputElement).value)"
    />

    <!-- 数字类型 -->
    <Input
      v-else-if="field.type === 'number' || field.type === 'integer'"
      :id="field.name"
      :value="value"
      :type="field.type === 'integer' ? 'number' : 'number'"
      :step="field.type === 'integer' ? '1' : 'any'"
      :min="field.minimum"
      :max="field.maximum"
      :placeholder="field.defaultValue ? String(field.defaultValue) : `输入 ${field.label}`"
      @input="handleNumberInput"
    />

    <!-- 布尔类型 -->
    <div v-else-if="field.type === 'boolean'" class="flex items-center space-x-2">
      <Checkbox :id="field.name" :checked="!!value" @update:checked="handleCheckboxChange" />
      <Label :for="field.name" class="cursor-pointer text-sm font-normal">
        {{ value ? '是' : '否' }}
      </Label>
    </div>

    <!-- 数组和对象类型 - 暂时显示提示 -->
    <div
      v-else-if="field.type === 'array' || field.type === 'object'"
      class="rounded-md border border-dashed border-muted-foreground/25 bg-muted/50 p-3 text-sm text-muted-foreground"
    >
      <p>复杂类型 ({{ field.type }}) 请切换到 <strong>JSON 模式</strong> 编辑</p>
    </div>

    <!-- 未知类型 -->
    <div v-else class="text-sm text-muted-foreground">不支持的类型: {{ field.type }}</div>

    <!-- 字段额外信息 -->
    <div
      v-if="field.minLength || field.maxLength || field.minimum || field.maximum"
      class="text-xs text-muted-foreground"
    >
      <span v-if="field.minLength">最小长度: {{ field.minLength }}</span>
      <span v-if="field.maxLength" class="ml-2">最大长度: {{ field.maxLength }}</span>
      <span v-if="field.minimum">最小值: {{ field.minimum }}</span>
      <span v-if="field.maximum" class="ml-2">最大值: {{ field.maximum }}</span>
    </div>
  </div>
</template>
