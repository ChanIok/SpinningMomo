<template>
  <div class="flex items-center justify-between gap-3 p-4">
    <!-- 左侧：搜索框 -->
    <div class="max-w-[400px] flex-1">
      <div class="relative">
        <Search class="absolute top-1/2 left-3 h-4 w-4 -translate-y-1/2 text-muted-foreground" />
        <Input
          :model-value="searchQuery"
          @update:model-value="updateSearchQuery"
          :placeholder="t('gallery.toolbar.search.placeholder')"
          class="pl-10"
        />
        <Button
          v-if="searchQuery"
          type="button"
          variant="sidebarGhost"
          size="icon-sm"
          class="absolute top-1/2 right-3 -translate-y-1/2"
          @click="clearSearch"
        >
          <X class="h-4 w-4" />
        </Button>
      </div>
    </div>

    <!-- 右侧：筛选、排序、视图控制 -->
    <div class="flex shrink-0 items-center gap-2">
      <!-- 筛选与排序下拉菜单 -->
      <TooltipProvider>
        <Tooltip>
          <TooltipTrigger as-child>
            <div>
              <DropdownMenu>
                <DropdownMenuTrigger as-child>
                  <Button variant="sidebarGhost" size="sm">
                    <ListFilter class="h-4 w-4" />
                  </Button>
                </DropdownMenuTrigger>
                <DropdownMenuContent align="end" class="w-56">
                  <!-- 类型筛选 -->
                  <DropdownMenuLabel>{{
                    t('gallery.toolbar.filter.type.label')
                  }}</DropdownMenuLabel>
                  <DropdownMenuRadioGroup
                    :model-value="filter.type || 'all'"
                    @update:model-value="onTypeFilterChange"
                  >
                    <DropdownMenuRadioItem value="all">
                      {{ t('gallery.toolbar.filter.type.all') }}
                    </DropdownMenuRadioItem>
                    <DropdownMenuRadioItem value="photo">
                      <Image class="mr-2 h-4 w-4" />
                      {{ t('gallery.toolbar.filter.type.photo') }}
                    </DropdownMenuRadioItem>
                    <DropdownMenuRadioItem value="video">
                      <Video class="mr-2 h-4 w-4" />
                      {{ t('gallery.toolbar.filter.type.video') }}
                    </DropdownMenuRadioItem>
                    <DropdownMenuRadioItem value="live_photo">
                      <Camera class="mr-2 h-4 w-4" />
                      {{ t('gallery.toolbar.filter.type.livePhoto') }}
                    </DropdownMenuRadioItem>
                  </DropdownMenuRadioGroup>

                  <DropdownMenuSeparator />

                  <!-- 排序方式 -->
                  <DropdownMenuLabel>{{ t('gallery.toolbar.sort.label') }}</DropdownMenuLabel>
                  <DropdownMenuRadioGroup
                    :model-value="sortBy"
                    @update:model-value="onSortByChange"
                  >
                    <DropdownMenuRadioItem value="createdAt">
                      <CalendarClock class="mr-2 h-4 w-4" />
                      {{ t('gallery.toolbar.sort.createdAt') }}
                    </DropdownMenuRadioItem>
                    <DropdownMenuRadioItem value="name">
                      <Type class="mr-2 h-4 w-4" />
                      {{ t('gallery.toolbar.sort.name') }}
                    </DropdownMenuRadioItem>
                    <DropdownMenuRadioItem value="resolution">
                      <Ruler class="mr-2 h-4 w-4" />
                      {{ t('gallery.toolbar.sort.resolution') }}
                    </DropdownMenuRadioItem>
                    <DropdownMenuRadioItem value="size">
                      <Ruler class="mr-2 h-4 w-4" />
                      {{ t('gallery.toolbar.sort.size') }}
                    </DropdownMenuRadioItem>
                  </DropdownMenuRadioGroup>

                  <DropdownMenuSeparator />

                  <!-- 排序顺序 -->
                  <DropdownMenuItem @click="toggleSortOrder">
                    <ArrowUpDown class="mr-2 h-4 w-4" />
                    <span>
                      {{
                        sortOrder === 'asc'
                          ? t('gallery.toolbar.sortOrder.asc')
                          : t('gallery.toolbar.sortOrder.desc')
                      }}
                    </span>
                  </DropdownMenuItem>

                  <DropdownMenuSeparator />

                  <!-- 文件夹选项 -->
                  <DropdownMenuLabel>{{
                    t('gallery.toolbar.folderOptions.label')
                  }}</DropdownMenuLabel>
                  <DropdownMenuCheckboxItem
                    :model-value="includeSubfolders"
                    @update:model-value="toggleIncludeSubfolders"
                  >
                    {{ t('gallery.toolbar.folderOptions.includeSubfolders') }}
                  </DropdownMenuCheckboxItem>
                </DropdownMenuContent>
              </DropdownMenu>
            </div>
          </TooltipTrigger>
          <TooltipContent>
            <p>{{ t('gallery.toolbar.filterAndSort.tooltip') }}</p>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>

      <TooltipProvider>
        <Tooltip>
          <TooltipTrigger as-child>
            <div>
              <Popover v-model:open="colorPopoverOpen">
                <PopoverTrigger as-child>
                  <Button variant="sidebarGhost" size="sm" class="relative">
                    <Palette class="h-4 w-4" />
                    <span
                      v-if="activeColorHex"
                      class="absolute right-1 bottom-1 h-2.5 w-2.5 rounded-full border border-background"
                      :style="{ backgroundColor: activeColorHex }"
                    />
                  </Button>
                </PopoverTrigger>
                <PopoverContent align="end" class="w-auto p-3">
                  <div class="w-[220px] space-y-3">
                    <div class="flex items-center justify-between gap-3">
                      <div class="flex min-w-0 items-center gap-2">
                        <div
                          class="h-5 w-5 shrink-0 rounded border border-border/80"
                          :style="{ backgroundColor: activeColorHex || draftColorHex }"
                        />
                        <div class="min-w-0">
                          <p class="text-xs font-medium">
                            {{ t('gallery.toolbar.colorFilter.title') }}
                          </p>
                          <p class="truncate font-mono text-[11px] text-muted-foreground">
                            {{ activeColorHex || t('gallery.toolbar.colorFilter.none') }}
                          </p>
                        </div>
                      </div>
                      <Button
                        v-if="activeColorHex"
                        variant="sidebarGhost"
                        size="sm"
                        class="h-7 px-2 text-xs"
                        @click="clearColorFilter"
                      >
                        {{ t('gallery.toolbar.colorFilter.clear') }}
                      </Button>
                    </div>

                    <ColorPicker
                      :model-value="draftColorHex"
                      @update:model-value="(color) => (draftColorHex = color)"
                    />

                    <div class="flex justify-end">
                      <Button size="sm" class="h-7 px-3 text-xs" @click="applyColorFilter">
                        {{ t('gallery.toolbar.colorFilter.apply') }}
                      </Button>
                    </div>
                  </div>
                </PopoverContent>
              </Popover>
            </div>
          </TooltipTrigger>
          <TooltipContent>
            <p>{{ t('gallery.toolbar.colorFilter.tooltip') }}</p>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>

      <!-- 评分与标记筛选 -->
      <TooltipProvider>
        <Tooltip>
          <TooltipTrigger as-child>
            <div>
              <Popover>
                <PopoverTrigger as-child>
                  <Button
                    variant="sidebarGhost"
                    size="sm"
                    class="relative"
                    :class="hasReviewFilter ? 'text-primary' : ''"
                  >
                    <Star class="h-4 w-4" />
                    <span
                      v-if="hasReviewFilter"
                      class="absolute right-1 bottom-1 h-2 w-2 rounded-full bg-primary"
                    />
                  </Button>
                </PopoverTrigger>
                <PopoverContent align="end" class="w-56 p-3">
                  <ReviewFilterPopover
                    :rating="filter.rating"
                    :review-flag="filter.reviewFlag"
                    @update:rating="(v) => galleryView.setFilter({ rating: v })"
                    @update:review-flag="(v) => galleryView.setFilter({ reviewFlag: v })"
                  />
                </PopoverContent>
              </Popover>
            </div>
          </TooltipTrigger>
          <TooltipContent>
            <p>{{ t('gallery.toolbar.filter.review.tooltip') }}</p>
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
                  <Button variant="sidebarGhost" size="sm">
                    <component :is="currentViewModeIcon" class="h-4 w-4" />
                  </Button>
                </PopoverTrigger>
                <PopoverContent align="end" class="w-72">
                  <div class="space-y-6">
                    <!-- 视图模式选择 -->
                    <div class="space-y-3">
                      <p class="text-sm font-medium">
                        {{ t('gallery.toolbar.viewMode.label') }}
                      </p>
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
                          <span class="text-xs">{{ t(mode.i18nKey) }}</span>
                        </Button>
                      </div>
                    </div>

                    <!-- 分隔线 -->
                    <div class="border-t" />

                    <!-- 缩略图大小调整 -->
                    <div class="space-y-3">
                      <div class="flex items-center">
                        <p class="text-sm font-medium">
                          {{ t('gallery.toolbar.thumbnailSize.label') }}
                        </p>
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
                        <span>{{ t('gallery.toolbar.thumbnailSize.fine') }}</span>
                        <span>{{ t('gallery.toolbar.thumbnailSize.showcase') }}</span>
                      </div>
                    </div>
                  </div>
                </PopoverContent>
              </Popover>
            </div>
          </TooltipTrigger>
          <TooltipContent>
            <p>{{ t('gallery.toolbar.viewSettings.tooltip') }}</p>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Slider } from '@/components/ui/slider'
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from '@/components/ui/tooltip'
import { Popover, PopoverContent, PopoverTrigger } from '@/components/ui/popover'
import ColorPicker from '@/components/ui/color-picker/ColorPicker.vue'
import ReviewFilterPopover from '../tags/ReviewFilterPopover.vue'
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
  Search,
  X,
  ArrowUpDown,
  Grid3x3,
  LayoutGrid,
  List,
  Rows3,
  ListFilter,
  Image,
  Video,
  Camera,
  CalendarClock,
  Palette,
  Type,
  Ruler,
  Star,
} from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import { useGalleryView } from '../../composables'
import type { ViewMode, SortBy, AssetType } from '../../types'

// i18n
const { t } = useI18n()

// 使用视图管理逻辑
const galleryView = useGalleryView()

// 计算属性
const viewMode = computed(() => galleryView.viewMode.value)
const sortBy = computed(() => galleryView.sortBy.value)
const sortOrder = computed(() => galleryView.sortOrder.value)
const filter = computed(() => galleryView.filter.value)
const searchQuery = computed(() => filter.value.searchQuery || '')
const includeSubfolders = computed(() => galleryView.includeSubfolders.value)
const activeColorHex = computed(() => filter.value.colorHex)

// 当前slider位置（从实际尺寸反向计算）
const currentSliderPosition = computed(() => galleryView.getSliderPosition())
const colorPopoverOpen = ref(false)
const draftColorHex = ref(activeColorHex.value || '#FFFFFF')

// 评分与标记筛选
const hasReviewFilter = computed(
  () => filter.value.rating !== undefined || filter.value.reviewFlag !== undefined
)

// 视图模式选项
const viewModes = [
  { value: 'grid' as ViewMode, icon: Grid3x3, i18nKey: 'gallery.toolbar.viewMode.grid' },
  { value: 'adaptive' as ViewMode, icon: Rows3, i18nKey: 'gallery.toolbar.viewMode.adaptive' },
  { value: 'masonry' as ViewMode, icon: LayoutGrid, i18nKey: 'gallery.toolbar.viewMode.masonry' },
  { value: 'list' as ViewMode, icon: List, i18nKey: 'gallery.toolbar.viewMode.list' },
]

// 当前视图模式的图标
const currentViewModeIcon = computed(() => {
  const mode = viewModes.find((m) => m.value === viewMode.value)
  return mode?.icon || Grid3x3
})

watch(colorPopoverOpen, (open) => {
  if (open) {
    draftColorHex.value = activeColorHex.value || '#FFFFFF'
  }
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

function applyColorFilter() {
  galleryView.setColorFilter(draftColorHex.value)
  colorPopoverOpen.value = false
}

function clearColorFilter() {
  galleryView.setColorFilter(undefined)
  draftColorHex.value = '#FFFFFF'
  colorPopoverOpen.value = false
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
