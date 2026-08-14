<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { ChevronLeft, Funnel, Menu, SlidersHorizontal, X } from '@lucide/vue'
import { Button } from '@/components/ui/button'
import { useI18n } from '@/composables/useI18n'
import {
  useGalleryFilterControls,
  useGalleryOverlayHistory,
  useGallerySelection,
  useGalleryViewControls,
} from '../../composables'
import { useGalleryStore } from '../../store'
import GalleryFilterSheet from './GalleryFilterSheet.vue'
import GalleryViewSettingsSheet from './GalleryViewSettingsSheet.vue'

const props = defineProps<{
  pageTitle: string
  touchLike: boolean
}>()

const emit = defineEmits<{
  back: []
  'toggle-sidebar': []
}>()

const { t } = useI18n()
const store = useGalleryStore()
const overlayHistory = useGalleryOverlayHistory()
const gallerySelection = useGallerySelection()
const { currentSource } = useGalleryViewControls()
const { activeFilterCount } = useGalleryFilterControls()
const filterOpen = ref(false)
const viewSettingsOpen = ref(false)
const isMultiSelectMode = computed(() => store.selection.mode === 'multi-select')

watch(
  () => overlayHistory.snapshot.value.overlay,
  (overlay) => {
    filterOpen.value = overlay === 'filter'
    viewSettingsOpen.value = overlay === 'view-settings'
  },
  { immediate: true }
)

function openFilterPanel() {
  void overlayHistory.openFilterPanel()
}

function openViewSettingsPanel() {
  void overlayHistory.openViewSettingsPanel()
}

function handleFilterOpenChange(open: boolean) {
  if (open) {
    openFilterPanel()
    return
  }
  void overlayHistory.closeFilterPanel()
}

function handleViewSettingsOpenChange(open: boolean) {
  if (open) {
    openViewSettingsPanel()
    return
  }
  void overlayHistory.closeViewSettingsPanel()
}

function exitMultiSelectMode() {
  void gallerySelection.exitMultiSelectMode()
}
</script>

<template>
  <div class="flex min-w-0 flex-1 items-center gap-0">
    <template v-if="isMultiSelectMode">
      <Button
        variant="ghost"
        size="icon"
        class="h-8 w-8 shrink-0 rounded-sm text-foreground/80 hover:bg-black/10 hover:text-foreground dark:hover:bg-white/10"
        :aria-label="t('gallery.mobile.selection.exit')"
        @click="exitMultiSelectMode"
      >
        <X class="size-4" :stroke-width="1.5" />
      </Button>
      <span class="min-w-0 truncate px-2 text-sm font-medium">
        <template v-if="store.selectedCount > 0">
          {{ t('gallery.mobile.selection.selectedCount', { count: store.selectedCount }) }}
        </template>
        <template v-else>
          {{ t('gallery.mobile.selection.empty') }}
        </template>
      </span>
      <div class="drag-region min-w-0 flex-1 self-stretch" aria-hidden="true" />
      <div class="h-8 w-8 shrink-0" aria-hidden="true" />
    </template>

    <template v-else>
      <Button
        variant="ghost"
        size="icon"
        class="h-8 w-8 shrink-0 rounded-sm text-foreground/80 hover:bg-black/10 hover:text-foreground dark:hover:bg-white/10"
        :aria-label="t('gallery.mobile.toolbar.menu')"
        @click="emit('toggle-sidebar')"
      >
        <Menu class="size-4" :stroke-width="1.5" />
      </Button>

      <Button
        v-if="!props.touchLike"
        variant="ghost"
        size="sm"
        class="h-8 max-w-24 shrink-0 gap-1 rounded-sm px-2 text-foreground/80 hover:bg-black/10 hover:text-foreground dark:hover:bg-white/10"
        :aria-label="t('app.navigation.back')"
        @click="emit('back')"
      >
        <ChevronLeft class="size-4 shrink-0" :stroke-width="1.5" />
        <span class="truncate text-xs font-medium">{{ props.pageTitle }}</span>
      </Button>

      <!-- 紧凑 Header 保留来源文字，但不重复显示文件夹/标签图标。 -->
      <div class="flex max-w-[45%] min-w-0 shrink px-1.5">
        <span class="min-w-0 truncate text-xs font-medium" :title="currentSource.label">
          {{ currentSource.label }}
        </span>
      </div>

      <div class="drag-region min-w-0 flex-1 self-stretch" aria-hidden="true" />

      <div class="flex shrink-0 items-center gap-0.5">
        <Button
          variant="ghost"
          size="icon"
          class="relative h-8 w-8 rounded-sm text-foreground/80 hover:bg-black/10 hover:text-foreground dark:hover:bg-white/10"
          :class="activeFilterCount > 0 ? 'text-primary hover:text-primary' : ''"
          :aria-label="t('gallery.mobile.toolbar.filter.title')"
          @click="openFilterPanel"
        >
          <Funnel class="size-4" :stroke-width="1.5" />
          <span
            v-if="activeFilterCount > 0"
            class="absolute top-0.5 right-0.5 flex h-3.5 min-w-3.5 items-center justify-center rounded-full bg-primary px-0.5 font-mono text-[8px] leading-none text-primary-foreground"
          >
            {{ activeFilterCount > 9 ? '9+' : activeFilterCount }}
          </span>
        </Button>

        <Button
          variant="ghost"
          size="icon"
          class="h-8 w-8 rounded-sm text-foreground/80 hover:bg-black/10 hover:text-foreground dark:hover:bg-white/10"
          :aria-label="t('gallery.mobile.toolbar.viewSettings.title')"
          @click="openViewSettingsPanel"
        >
          <SlidersHorizontal class="size-4" :stroke-width="1.5" />
        </Button>
      </div>
    </template>
  </div>

  <GalleryFilterSheet :open="filterOpen" @update:open="handleFilterOpenChange" />
  <GalleryViewSettingsSheet :open="viewSettingsOpen" @update:open="handleViewSettingsOpenChange" />
</template>

<style scoped>
.drag-region {
  -webkit-app-region: drag;
  app-region: drag;
}
</style>
