<template>
  <div class="flex items-center gap-4 border-b bg-background p-4">
    <!-- 快速操作按钮 -->
    <div class="flex items-center gap-2">
      <TooltipProvider>
        <Tooltip v-if="hasSelection">
          <TooltipTrigger as-child>
            <Button variant="outline" size="sm" @click="$emit('deleteSelected')">
              <Trash2 class="mr-2 h-4 w-4" />
              删除选中 ({{ selectedCount }})
            </Button>
          </TooltipTrigger>
          <TooltipContent>
            <p>删除选中的 {{ selectedCount }} 项资产</p>
          </TooltipContent>
        </Tooltip>

        <Tooltip>
          <TooltipTrigger as-child>
            <Button variant="outline" size="sm" @click="$emit('refresh')" :disabled="isLoading">
              <RefreshCw class="mr-2 h-4 w-4" :class="{ 'animate-spin': isLoading }" />
              {{ isLoading ? '刷新中...' : '刷新' }}
            </Button>
          </TooltipTrigger>
          <TooltipContent>
            <p>重新加载图库资产</p>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>
    </div>

    <Separator orientation="vertical" class="h-6" />

    <!-- 搜索框 -->
    <div class="max-w-[400px] min-w-[200px] flex-1">
      <div class="relative">
        <Search class="absolute top-1/2 left-3 h-4 w-4 -translate-y-1/2 text-muted-foreground" />
        <Input
          :model-value="searchQuery"
          @update:model-value="updateSearchQuery"
          placeholder="搜索资产名称..."
          class="pl-10"
        />
        <button
          v-if="searchQuery"
          class="absolute top-1/2 right-3 -translate-y-1/2 text-muted-foreground hover:text-foreground"
          @click="clearSearch"
        >
          <X class="h-4 w-4" />
        </button>
      </div>
    </div>

    <Separator orientation="vertical" class="h-6" />

    <!-- 类型筛选 -->
    <div class="flex items-center gap-2">
      <span class="text-sm font-medium">类型:</span>
      <Select :model-value="filter.type || 'all'" @update:model-value="onTypeFilterChange">
        <SelectTrigger class="w-[140px]">
          <SelectValue placeholder="选择类型" />
        </SelectTrigger>
        <SelectContent>
          <SelectItem value="all">全部</SelectItem>
          <SelectItem value="photo">📷 照片</SelectItem>
          <SelectItem value="video">🎥 视频</SelectItem>
          <SelectItem value="live_photo">📸 实况</SelectItem>
        </SelectContent>
      </Select>
    </div>

    <Separator orientation="vertical" class="h-6" />

    <!-- 排序控制 -->
    <div class="flex items-center gap-2">
      <span class="text-sm font-medium">排序:</span>
      <Select :model-value="sortBy" @update:model-value="onSortByChange">
        <SelectTrigger class="w-[140px]">
          <SelectValue placeholder="选择排序" />
        </SelectTrigger>
        <SelectContent>
          <SelectItem value="createdAt">📅 创建日期</SelectItem>
          <SelectItem value="name">📝 名称</SelectItem>
          <SelectItem value="size">📏 大小</SelectItem>
        </SelectContent>
      </Select>

      <TooltipProvider>
        <Tooltip>
          <TooltipTrigger as-child>
            <Button variant="outline" size="sm" @click="toggleSortOrder">
              <ArrowUpDown class="h-4 w-4" :class="{ 'rotate-180': sortOrder === 'desc' }" />
              <span class="ml-1">{{ sortOrder === 'asc' ? '升序' : '降序' }}</span>
            </Button>
          </TooltipTrigger>
          <TooltipContent>
            <p>切换排序顺序</p>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>
    </div>

    <Separator orientation="vertical" class="h-6" />

    <!-- 视图控制 -->
    <div class="flex items-center gap-2">
      <span class="text-sm font-medium">视图:</span>

      <!-- 视图模式切换 -->
      <ToggleGroup type="single" :model-value="viewMode" @update:model-value="setViewMode">
        <TooltipProvider>
          <Tooltip v-for="mode in viewModes" :key="mode.value">
            <TooltipTrigger as-child>
              <ToggleGroupItem :value="mode.value" aria-label="Toggle bold">
                <component :is="mode.icon" class="h-4 w-4" />
                <span class="ml-1 hidden sm:inline">{{ mode.label }}</span>
              </ToggleGroupItem>
            </TooltipTrigger>
            <TooltipContent>
              <p>{{ mode.label }}视图</p>
            </TooltipContent>
          </Tooltip>
        </TooltipProvider>
      </ToggleGroup>

      <!-- 视图大小调节 -->
      <div class="flex items-center gap-1">
        <TooltipProvider>
          <Tooltip>
            <TooltipTrigger as-child>
              <Button variant="outline" size="sm" @click="decreaseSize" :disabled="viewSize <= 1">
                <Minus class="h-4 w-4" />
              </Button>
            </TooltipTrigger>
            <TooltipContent>
              <p>缩小</p>
            </TooltipContent>
          </Tooltip>

          <span class="min-w-[3rem] text-center text-sm">{{ viewSize }}/5</span>

          <Tooltip>
            <TooltipTrigger as-child>
              <Button variant="outline" size="sm" @click="increaseSize" :disabled="viewSize >= 5">
                <Plus class="h-4 w-4" />
              </Button>
            </TooltipTrigger>
            <TooltipContent>
              <p>放大</p>
            </TooltipContent>
          </Tooltip>
        </TooltipProvider>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Separator } from '@/components/ui/separator'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { ToggleGroup, ToggleGroupItem } from '@/components/ui/toggle-group'
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from '@/components/ui/tooltip'
import {
  Trash2,
  RefreshCw,
  Search,
  X,
  ArrowUpDown,
  Grid3x3,
  LayoutGrid,
  List,
  Rows3,
  Minus,
  Plus,
} from 'lucide-vue-next'
import { useGalleryView } from '../composables'
import type { ViewMode, SortBy, AssetType } from '../types'

// Props 定义
interface GalleryToolbarProps {
  isLoading?: boolean
  selectedCount?: number
}

const props = withDefaults(defineProps<GalleryToolbarProps>(), {
  isLoading: false,
  selectedCount: 0,
})

// Emits 定义
const emit = defineEmits<{
  refresh: []
  deleteSelected: []
}>()

// 使用视图管理逻辑
const galleryView = useGalleryView()

// 计算属性
const viewMode = computed(() => galleryView.viewMode.value)
const viewSize = computed(() => galleryView.viewSize.value)
const sortBy = computed(() => galleryView.sortBy.value)
const sortOrder = computed(() => galleryView.sortOrder.value)
const filter = computed(() => galleryView.filter.value)
const searchQuery = computed(() => filter.value.searchQuery || '')

const hasSelection = computed(() => props.selectedCount > 0)

// 视图模式选项
const viewModes = [
  { value: 'grid' as ViewMode, icon: Grid3x3, label: '网格' },
  { value: 'masonry' as ViewMode, icon: LayoutGrid, label: '瀑布流' },
  { value: 'list' as ViewMode, icon: List, label: '列表' },
  { value: 'adaptive' as ViewMode, icon: Rows3, label: '自适应' },
]

// 方法
function updateSearchQuery(query: string | number) {
  galleryView.setSearchQuery(String(query))
}

function clearSearch() {
  galleryView.setSearchQuery('')
}

function onTypeFilterChange(value: string | number | bigint | Record<string, any> | null) {
  const stringValue = String(value || 'all')
  const type = stringValue === 'all' ? undefined : (stringValue as AssetType)
  galleryView.setTypeFilter(type)
}

function onSortByChange(value: string | number | bigint | Record<string, any> | null) {
  if (value) {
    const newSortBy = String(value) as SortBy
    galleryView.setSorting(newSortBy, sortOrder.value)
  }
}

function toggleSortOrder() {
  galleryView.toggleSortOrder()
}

function setViewMode(
  mode:
    | string
    | number
    | bigint
    | Record<string, any>
    | null
    | (string | number | bigint | Record<string, any> | null)[]
) {
  if (mode && typeof mode === 'string') {
    galleryView.setViewMode(mode as ViewMode)
  }
}

function increaseSize() {
  galleryView.increaseSize()
}

function decreaseSize() {
  galleryView.decreaseSize()
}
</script>
