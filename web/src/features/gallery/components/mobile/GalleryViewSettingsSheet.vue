<script setup lang="ts">
import { computed } from 'vue'
import { ArrowDown, ArrowUp } from '@lucide/vue'
import { MobileDrawer } from '@/components/ui/mobile-drawer'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Slider } from '@/components/ui/slider'
import { useI18n } from '@/composables/useI18n'
import { useGalleryViewControls } from '../../composables'

defineProps<{ open: boolean }>()
const emit = defineEmits<{ 'update:open': [value: boolean] }>()

const { t } = useI18n()
const {
  viewMode,
  sortBy,
  sortOrder,
  includeSubfolders,
  setIncludeSubfolders,
  currentSliderPosition,
  availableViewModes,
  onSortByChange,
  toggleSortOrder,
  setViewMode,
  onViewSizeSliderChange,
} = useGalleryViewControls()

const sortOptions = computed(() => [
  { value: 'createdAt', label: t('gallery.toolbar.sort.createdAt') },
  { value: 'name', label: t('gallery.toolbar.sort.name') },
  { value: 'resolution', label: t('gallery.toolbar.sort.resolution') },
  { value: 'size', label: t('gallery.toolbar.sort.size') },
])
</script>

<template>
  <MobileDrawer
    :open="open"
    side="bottom"
    class="max-h-[88vh] rounded-t-2xl border-t border-border/40 text-sidebar-foreground supports-[height:100dvh]:max-h-[88dvh]"
    @close="emit('update:open', false)"
  >
    <!-- 滚动内容区 -->
    <ScrollArea class="min-h-0 flex-1">
      <!-- 滑块位于内容末尾，需要额外的下方触控余量，避免手指贴近抽屉边缘。 -->
      <div class="flex flex-col gap-4 px-4 pt-2 pb-9">
        <!-- 视图模式（3联分段控制器） -->
        <div class="flex flex-col gap-2">
          <div class="flex h-5 items-center justify-between">
            <span class="text-sm font-medium text-foreground">
              {{ t('gallery.toolbar.viewMode.label') }}
            </span>
          </div>
          <div
            class="grid grid-cols-3 gap-1 rounded-lg border border-border/30 bg-sidebar-hover/50 p-1"
          >
            <button
              v-for="mode in availableViewModes"
              :key="mode.value"
              type="button"
              class="flex h-8.5 items-center justify-center gap-1.5 rounded-md text-xs font-medium transition-colors duration-150"
              :class="
                viewMode === mode.value
                  ? 'bg-sidebar-accent font-medium text-primary shadow-xs [&_svg]:text-primary'
                  : 'text-sidebar-foreground hover:bg-sidebar-hover hover:text-sidebar-accent-foreground'
              "
              @click="setViewMode(mode.value)"
            >
              <component :is="mode.icon" class="size-3.5" />
              <span>{{ t(mode.i18nKey) }}</span>
            </button>
          </div>
        </div>

        <!-- 排序方式（4联分段器 + 排序方向切换） -->
        <div class="flex flex-col gap-2">
          <div class="flex h-5 items-center justify-between">
            <span class="text-sm font-medium text-foreground">
              {{ t('gallery.toolbar.sort.label') }}
            </span>
            <button
              type="button"
              class="flex items-center gap-1 text-xs font-medium text-primary transition-opacity hover:opacity-85"
              @click="toggleSortOrder"
            >
              <ArrowDown v-if="sortOrder === 'desc'" class="size-3.5" />
              <ArrowUp v-else class="size-3.5" />
              <span>{{
                sortOrder === 'desc'
                  ? t('gallery.toolbar.sortOrder.desc')
                  : t('gallery.toolbar.sortOrder.asc')
              }}</span>
            </button>
          </div>
          <div
            class="grid grid-cols-4 gap-1 rounded-lg border border-border/30 bg-sidebar-hover/50 p-1"
          >
            <button
              v-for="opt in sortOptions"
              :key="opt.value"
              type="button"
              class="flex h-8.5 items-center justify-center rounded-md text-xs font-medium transition-colors duration-150"
              :class="
                sortBy === opt.value
                  ? 'bg-sidebar-accent font-medium text-primary shadow-xs'
                  : 'text-sidebar-foreground hover:bg-sidebar-hover hover:text-sidebar-accent-foreground'
              "
              @click="onSortByChange(opt.value)"
            >
              <span>{{ opt.label }}</span>
            </button>
          </div>
        </div>

        <!-- 文件夹范围（2联分段控制器） -->
        <div class="flex flex-col gap-2">
          <div class="flex h-5 items-center justify-between">
            <span class="text-sm font-medium text-foreground">
              {{ t('gallery.toolbar.filters.folderScope') }}
            </span>
          </div>
          <div
            class="grid grid-cols-2 gap-1 rounded-lg border border-border/30 bg-sidebar-hover/50 p-1"
          >
            <button
              type="button"
              class="flex h-8.5 items-center justify-center rounded-md text-xs font-medium transition-colors duration-150"
              :class="
                includeSubfolders
                  ? 'bg-sidebar-accent font-medium text-primary shadow-xs'
                  : 'text-sidebar-foreground hover:bg-sidebar-hover hover:text-sidebar-accent-foreground'
              "
              @click="setIncludeSubfolders(true)"
            >
              <span>{{ t('gallery.toolbar.filter.subfolders.include') }}</span>
            </button>
            <button
              type="button"
              class="flex h-8.5 items-center justify-center rounded-md text-xs font-medium transition-colors duration-150"
              :class="
                !includeSubfolders
                  ? 'bg-sidebar-accent font-medium text-primary shadow-xs'
                  : 'text-sidebar-foreground hover:bg-sidebar-hover hover:text-sidebar-accent-foreground'
              "
              @click="setIncludeSubfolders(false)"
            >
              <span>{{ t('gallery.toolbar.filter.subfolders.currentOnly') }}</span>
            </button>
          </div>
        </div>

        <!-- 缩略图大小（紧凑滑块） -->
        <div class="flex flex-col gap-2">
          <div class="flex h-5 items-center justify-between">
            <span class="text-sm font-medium text-foreground">
              {{ t('gallery.toolbar.thumbnailSize.label') }}
            </span>
            <span class="font-mono text-xs font-medium text-primary"
              >{{ currentSliderPosition }}%</span
            >
          </div>
          <Slider
            :model-value="[currentSliderPosition]"
            :min="0"
            :max="100"
            :step="1"
            @update:model-value="onViewSizeSliderChange"
          />
        </div>
      </div>
    </ScrollArea>
  </MobileDrawer>
</template>
