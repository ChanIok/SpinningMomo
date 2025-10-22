<template>
  <div class="flex items-center gap-3 bg-background p-4">
    <!-- 左侧：快速操作 -->
    <div class="flex items-center gap-2">
      <TooltipProvider>
        <Tooltip v-if="hasSelection">
          <TooltipTrigger as-child>
            <Button variant="ghost" size="sm" @click="$emit('deleteSelected')">
              <Trash2 class="h-4 w-4" />
              <span class="ml-1.5 hidden sm:inline">删除 ({{ selectedCount }})</span>
            </Button>
          </TooltipTrigger>
          <TooltipContent>
            <p>删除选中的 {{ selectedCount }} 项资产</p>
          </TooltipContent>
        </Tooltip>

        <Tooltip>
          <TooltipTrigger as-child>
            <Button variant="ghost" size="sm" @click="$emit('refresh')" :disabled="isLoading">
              <RefreshCw class="h-4 w-4" :class="{ 'animate-spin': isLoading }" />
            </Button>
          </TooltipTrigger>
          <TooltipContent>
            <p>重新加载图库资产</p>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>
    </div>

    <!-- 中间：搜索框 -->
    <div class="max-w-md min-w-[200px] flex-1">
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
          class="absolute top-1/2 right-3 -translate-y-1/2 text-muted-foreground transition-colors hover:text-foreground"
          @click="clearSearch"
        >
          <X class="h-4 w-4" />
        </button>
      </div>
    </div>

    <!-- 右侧：筛选、排序、视图控制 -->
    <div class="flex items-center gap-2">
      <!-- 筛选与排序下拉菜单 -->
      <TooltipProvider>
        <Tooltip>
          <TooltipTrigger as-child>
            <div>
              <DropdownMenu>
                <DropdownMenuTrigger as-child>
                  <Button variant="ghost" size="sm">
                    <SlidersHorizontal class="h-4 w-4" />
                  </Button>
                </DropdownMenuTrigger>
                <DropdownMenuContent align="end" class="w-56">
                  <!-- 类型筛选 -->
                  <DropdownMenuLabel>资产类型</DropdownMenuLabel>
                  <DropdownMenuRadioGroup
                    :model-value="filter.type || 'all'"
                    @update:model-value="onTypeFilterChange"
                  >
                    <DropdownMenuRadioItem value="all">全部类型</DropdownMenuRadioItem>
                    <DropdownMenuRadioItem value="photo">📷 照片</DropdownMenuRadioItem>
                    <DropdownMenuRadioItem value="video">🎥 视频</DropdownMenuRadioItem>
                    <DropdownMenuRadioItem value="live_photo">📸 实况</DropdownMenuRadioItem>
                  </DropdownMenuRadioGroup>

                  <DropdownMenuSeparator />

                  <!-- 排序方式 -->
                  <DropdownMenuLabel>排序方式</DropdownMenuLabel>
                  <DropdownMenuRadioGroup
                    :model-value="sortBy"
                    @update:model-value="onSortByChange"
                  >
                    <DropdownMenuRadioItem value="createdAt">📅 创建日期</DropdownMenuRadioItem>
                    <DropdownMenuRadioItem value="name">📝 名称</DropdownMenuRadioItem>
                    <DropdownMenuRadioItem value="size">📏 大小</DropdownMenuRadioItem>
                  </DropdownMenuRadioGroup>

                  <DropdownMenuSeparator />

                  <!-- 排序顺序 -->
                  <DropdownMenuItem @click="toggleSortOrder">
                    <ArrowUpDown class="mr-2 h-4 w-4" />
                    <span>{{ sortOrder === 'asc' ? '升序排列' : '降序排列' }}</span>
                  </DropdownMenuItem>

                  <DropdownMenuSeparator />

                  <!-- 文件夹选项 -->
                  <DropdownMenuLabel>文件夹选项</DropdownMenuLabel>
                  <DropdownMenuCheckboxItem
                    :model-value="includeSubfolders"
                    @update:model-value="toggleIncludeSubfolders"
                  >
                    📂 包含子文件夹
                  </DropdownMenuCheckboxItem>
                </DropdownMenuContent>
              </DropdownMenu>
            </div>
          </TooltipTrigger>
          <TooltipContent>
            <p>筛选与排序</p>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>

      <!-- 视图设置（模式 + 大小调整） -->
      <TooltipProvider>
        <Tooltip>
          <TooltipTrigger as-child>
            <div>
              <Popover>
                <PopoverTrigger as-child>
                  <Button variant="ghost" size="sm">
                    <component :is="currentViewModeIcon" class="h-4 w-4" />
                  </Button>
                </PopoverTrigger>
                <PopoverContent align="end" class="w-72">
                  <div class="space-y-6">
                    <!-- 视图模式选择 -->
                    <div class="space-y-3">
                      <p class="text-sm font-medium">视图模式</p>
                      <div class="grid grid-cols-4 gap-2">
                        <Button
                          v-for="mode in viewModes"
                          :key="mode.value"
                          :variant="viewMode === mode.value ? 'default' : 'outline'"
                          size="sm"
                          class="flex h-auto flex-col items-center gap-1.5 py-3"
                          @click="setViewMode(mode.value)"
                        >
                          <component :is="mode.icon" class="h-5 w-5" />
                          <span class="text-xs">{{ mode.label }}</span>
                        </Button>
                      </div>
                    </div>

                    <!-- 分隔线 -->
                    <div class="border-t" />

                    <!-- 缩略图大小调整 -->
                    <div class="space-y-3">
                      <div class="flex items-center justify-between">
                        <p class="text-sm font-medium">缩略图大小</p>
                        <span class="text-sm text-muted-foreground">{{ viewSize }}px</span>
                      </div>
                      <Slider
                        :model-value="[currentSliderPosition]"
                        @update:model-value="onViewSizeSliderChange"
                        :min="0"
                        :max="100"
                        :step="1"
                        class="w-full"
                      />
                      <div class="flex justify-between text-xs text-muted-foreground">
                        <span>精致</span>
                        <span>展示</span>
                      </div>
                    </div>
                  </div>
                </PopoverContent>
              </Popover>
            </div>
          </TooltipTrigger>
          <TooltipContent>
            <p>视图设置</p>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Slider } from '@/components/ui/slider'
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from '@/components/ui/tooltip'
import { Popover, PopoverContent, PopoverTrigger } from '@/components/ui/popover'
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuLabel,
  DropdownMenuRadioGroup,
  DropdownMenuRadioItem,
  DropdownMenuSeparator,
  DropdownMenuTrigger,
  DropdownMenuCheckboxItem,
} from '@/components/ui/dropdown-menu'
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
  SlidersHorizontal,
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
const includeSubfolders = computed(() => galleryView.includeSubfolders.value)

// 当前slider位置（从实际尺寸反向计算）
const currentSliderPosition = computed(() => galleryView.getSliderPosition())

const hasSelection = computed(() => props.selectedCount > 0)

// 视图模式选项
const viewModes = [
  { value: 'grid' as ViewMode, icon: Grid3x3, label: '网格' },
  { value: 'masonry' as ViewMode, icon: LayoutGrid, label: '瀑布流' },
  { value: 'list' as ViewMode, icon: List, label: '列表' },
  { value: 'adaptive' as ViewMode, icon: Rows3, label: '自适应' },
]

// 当前视图模式的图标
const currentViewModeIcon = computed(() => {
  const mode = viewModes.find((m) => m.value === viewMode.value)
  return mode?.icon || Grid3x3
})

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

function toggleIncludeSubfolders() {
  galleryView.setIncludeSubfolders(!includeSubfolders.value)
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

function onViewSizeSliderChange(value: number[] | undefined) {
  if (value && value.length > 0 && value[0] !== undefined) {
    // 使用非线性映射函数设置尺寸
    galleryView.setViewSizeFromSlider(value[0])
  }
}
</script>
