<script setup lang="ts">
import { ref, computed } from 'vue'
import { PlayIcon, SearchIcon } from 'lucide-vue-next'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import type { ApiMethod } from '../types'

interface Props {
  methods: ApiMethod[]
  loading?: boolean
  selectedMethod?: string | null
}

interface Emits {
  (e: 'select', method: ApiMethod): void
  (e: 'refresh'): void
}

const props = withDefaults(defineProps<Props>(), {
  loading: false,
  selectedMethod: null,
})

const emit = defineEmits<Emits>()

const searchQuery = ref('')

// 按分类分组的方法
const methodsByCategory = computed(() => {
  const grouped: Record<string, ApiMethod[]> = {}

  const filteredMethods = props.methods.filter((method) => {
    if (!searchQuery.value.trim()) return true

    const query = searchQuery.value.toLowerCase()
    return (
      method.name.toLowerCase().includes(query) ||
      method.category?.toLowerCase().includes(query) ||
      method.description?.toLowerCase().includes(query)
    )
  })

  filteredMethods.forEach((method) => {
    const category = method.category || '未分类'
    if (!grouped[category]) {
      grouped[category] = []
    }
    grouped[category].push(method)
  })

  return grouped
})

// 所有分类
const categories = computed(() => {
  return Object.keys(methodsByCategory.value).sort()
})

const handleSelectMethod = (method: ApiMethod) => {
  emit('select', method)
}

const handleRefresh = () => {
  emit('refresh')
}
</script>

<template>
  <div class="flex h-full flex-col">
    <!-- 搜索和刷新 -->
    <div class="space-y-2 border-b p-3">
      <div class="relative">
        <SearchIcon
          class="absolute top-1/2 left-3 size-3 -translate-y-1/2 transform text-muted-foreground"
        />
        <Input v-model="searchQuery" placeholder="搜索API方法..." class="h-8 pl-8 text-sm" />
      </div>
      <Button
        @click="handleRefresh"
        :disabled="loading"
        variant="outline"
        size="sm"
        class="h-7 w-full text-xs"
      >
        <SearchIcon v-if="!loading" class="mr-1 size-3" />
        <div
          v-else
          class="mr-1 size-3 animate-spin rounded-full border-2 border-current border-t-transparent"
        />
        {{ loading ? '获取中...' : '刷新' }}
      </Button>
    </div>

    <!-- 方法列表 -->
    <div class="flex-1 overflow-y-auto">
      <div v-if="loading && methods.length === 0" class="flex h-20 items-center justify-center">
        <div class="text-xs text-muted-foreground">正在加载API方法...</div>
      </div>

      <div
        v-else-if="Object.keys(methodsByCategory).length === 0"
        class="flex h-20 items-center justify-center"
      >
        <div class="text-xs text-muted-foreground">
          {{ searchQuery ? '未找到匹配的方法' : '暂无API方法' }}
        </div>
      </div>

      <div v-else class="space-y-2 p-1">
        <!-- 按分类显示 -->
        <div v-for="category in categories" :key="category" class="space-y-1">
          <!-- 分类标题 -->
          <div
            class="sticky top-0 bg-background px-2 py-0.5 text-xs font-medium text-muted-foreground"
          >
            {{ category }}
            <span class="ml-1 text-xs text-muted-foreground"
              >({{ methodsByCategory[category]?.length || 0 }})</span
            >
          </div>

          <!-- 方法列表 - 使用更紧凑的布局 -->
          <div class="space-y-1 px-1">
            <div
              v-for="method in methodsByCategory[category]"
              :key="method.name"
              :class="[
                'flex cursor-pointer items-center gap-2 rounded px-2 py-1 text-xs hover:bg-accent/50',
                selectedMethod === method.name && 'bg-accent',
              ]"
              @click="handleSelectMethod(method)"
            >
              <PlayIcon class="size-3 flex-shrink-0 text-muted-foreground" />
              <div class="min-w-0 flex-1 truncate font-medium">
                {{ method.name }}
              </div>
              <Button
                variant="ghost"
                size="sm"
                class="h-5 px-1 text-xs opacity-0 transition-opacity hover:opacity-100"
                @click.stop="handleSelectMethod(method)"
              >
                测试
              </Button>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>
