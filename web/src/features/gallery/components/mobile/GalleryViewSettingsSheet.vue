<script setup lang="ts">
import { computed } from 'vue'
import { ArrowDown, ArrowUp, Folder } from '@lucide/vue'
import { MobileDrawer } from '@/components/ui/mobile-drawer'
import { Slider } from '@/components/ui/slider'
import { Switch } from '@/components/ui/switch'
import { useI18n } from '@/composables/useI18n'
import { useGalleryViewControls } from '../../composables'

defineProps<{ open: boolean }>()
const emit = defineEmits<{ 'update:open': [value: boolean] }>()

const { t } = useI18n()
const {
  viewMode,
  sortBy,
  sortOrder,
  currentFolderOnly,
  currentSliderPosition,
  availableViewModes,
  onSortByChange,
  toggleSortOrder,
  onCurrentFolderOnlyChange,
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
    class="max-h-[88vh] rounded-t-2xl border-t border-border/40 px-4 pt-1 pb-[calc(env(safe-area-inset-bottom)+1.25rem)] text-sidebar-foreground"
    @close="emit('update:open', false)"
  >
    <!-- 抽屉头部（居中标题） -->
    <div class="flex h-8 shrink-0 items-center justify-center border-b border-border/40 pb-1">
      <span class="text-sm font-medium text-foreground">
        {{ t('gallery.mobile.toolbar.viewSettings.title') }}
      </span>
    </div>

    <!-- 设置内容列表 -->
    <div class="min-h-0 flex-1 space-y-4.5 overflow-y-auto pt-3.5 pb-1">
      <!-- 视图模式（3联分段控制器） -->
      <div class="space-y-2">
        <label class="text-sm font-medium text-foreground">
          {{ t('gallery.toolbar.viewMode.label') }}
        </label>
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
      <div class="space-y-2">
        <div class="flex items-center justify-between">
          <label class="text-sm font-medium text-foreground">
            {{ t('gallery.toolbar.sort.label') }}
          </label>
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

      <!-- 缩略图大小（紧凑滑块） -->
      <div class="space-y-2">
        <div class="flex items-center justify-between">
          <label class="text-sm font-medium text-foreground">
            {{ t('gallery.toolbar.thumbnailSize.label') }}
          </label>
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
        <div class="flex justify-between pt-0.5 text-xs text-muted-foreground">
          <span>{{ t('gallery.toolbar.thumbnailSize.fine') }}</span>
          <span>{{ t('gallery.toolbar.thumbnailSize.showcase') }}</span>
        </div>
      </div>

      <!-- 文件夹选项（卡片 Switch 开关行） -->
      <div
        class="flex cursor-pointer items-center justify-between rounded-lg border border-border/40 bg-sidebar-hover/30 p-3 transition-colors duration-150 hover:bg-sidebar-hover/50"
        @click="onCurrentFolderOnlyChange(!currentFolderOnly)"
      >
        <div class="flex min-w-0 items-center gap-2.5">
          <Folder class="size-4 shrink-0 text-muted-foreground" />
          <span class="text-sm font-medium text-foreground">
            {{ t('gallery.toolbar.folderOptions.currentFolderOnly') }}
          </span>
        </div>
        <Switch
          :checked="currentFolderOnly"
          @click.stop
          @update:checked="onCurrentFolderOnlyChange"
        />
      </div>
    </div>
  </MobileDrawer>
</template>
