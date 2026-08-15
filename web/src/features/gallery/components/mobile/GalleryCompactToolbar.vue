<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { Funnel, Menu, SlidersHorizontal, X } from '@lucide/vue'
import { Button } from '@/components/ui/button'
import { useI18n } from '@/composables/useI18n'
import {
  useGalleryFilterControls,
  useGalleryOverlayHistory,
  useGallerySelection,
} from '../../composables'
import { useGalleryStore } from '../../store'
import GalleryFilterSheet from './GalleryFilterSheet.vue'
import GalleryViewSettingsSheet from './GalleryViewSettingsSheet.vue'

const { t } = useI18n()
const store = useGalleryStore()
const overlayHistory = useGalleryOverlayHistory()
const gallerySelection = useGallerySelection()
const { activeFilterCount } = useGalleryFilterControls()
const filterOpen = ref(false)
const viewSettingsOpen = ref(false)
const isMultiSelectMode = computed(() => store.selection.mode === 'multi-select')

const isVisible = computed(() => {
  if (isMultiSelectMode.value || filterOpen.value || viewSettingsOpen.value) {
    return true
  }
  return store.compactToolbarVisible
})

const hasBackground = computed(() => {
  if (isMultiSelectMode.value || filterOpen.value || viewSettingsOpen.value) {
    return true
  }
  return store.compactToolbarWithBackground
})

const surfaceClass = computed(() => {
  if (hasBackground.value) {
    return 'bg-background/75 dark:bg-popover/75  text-foreground'
  }
  return 'bg-transparent text-foreground/10'
})

watch(
  () => overlayHistory.snapshot.value.overlay,
  (overlay) => {
    filterOpen.value = overlay === 'filter'
    viewSettingsOpen.value = overlay === 'view-settings'
  },
  { immediate: true }
)

function openFolderDrawer() {
  if (overlayHistory.snapshot.value.overlay === 'folder') {
    void overlayHistory.closeFolderDrawer()
  } else {
    void overlayHistory.openFolderDrawer()
  }
}

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
  <div
    class="pointer-events-none flex h-12 w-full items-center justify-between bg-transparent px-2.5 transition-all duration-260 ease-[cubic-bezier(0.16,1,0.3,1)]"
    :class="[isVisible ? 'translate-y-0 opacity-100' : '-translate-y-full opacity-0']"
  >
    <template v-if="isMultiSelectMode">
      <div
        class="pointer-events-auto flex min-w-0 items-center gap-1.5 rounded-full px-1 py-0.5 transition-all"
        :class="surfaceClass"
      >
        <Button
          variant="ghost"
          size="icon"
          class="h-9 w-9 shrink-0 rounded-full text-foreground transition-colors hover:bg-black/10 active:bg-black/15 dark:hover:bg-white/10 dark:active:bg-white/15"
          :aria-label="t('gallery.mobile.selection.exit')"
          @click="exitMultiSelectMode"
        >
          <X class="size-4.5" :stroke-width="1.5" />
        </Button>
        <span class="min-w-0 truncate pr-3 pl-0.5 text-xs font-medium">
          <template v-if="store.selectedCount > 0">
            {{ t('gallery.mobile.selection.selectedCount', { count: store.selectedCount }) }}
          </template>
          <template v-else>
            {{ t('gallery.mobile.selection.empty') }}
          </template>
        </span>
      </div>
    </template>

    <template v-else>
      <!-- 左侧：文件夹/侧栏菜单正方形自然圆角按钮 -->
      <div
        class="pointer-events-auto flex h-10 w-10 shrink-0 items-center justify-center rounded-xl transition-all"
        :class="surfaceClass"
      >
        <Button
          variant="ghost"
          size="icon"
          class="h-10 w-10 text-foreground transition-colors hover:bg-black/10 active:bg-black/15 dark:hover:bg-white/10 dark:active:bg-white/15"
          :aria-label="t('gallery.mobile.toolbar.menu')"
          @click="openFolderDrawer"
        >
          <Menu class="size-5" :stroke-width="1.5" />
        </Button>
      </div>

      <!-- 右侧：筛选与排序/视图设置聚合药丸胶囊 -->
      <div
        class="pointer-events-auto flex h-10 items-center gap-0.5 rounded-full p-0.5 transition-all"
        :class="surfaceClass"
      >
        <Button
          variant="ghost"
          size="icon"
          class="relative h-9 w-11 shrink-0 rounded-full text-foreground transition-colors hover:bg-black/10 active:bg-black/15 dark:hover:bg-white/10 dark:active:bg-white/15"
          :class="activeFilterCount > 0 ? 'text-primary hover:text-primary' : ''"
          :aria-label="t('gallery.mobile.toolbar.filter.title')"
          @click="openFilterPanel"
        >
          <Funnel class="size-4.5" :stroke-width="1.5" />
          <span
            v-if="activeFilterCount > 0"
            class="pointer-events-none absolute top-1 right-1 flex h-3.5 min-w-3.5 items-center justify-center rounded-full bg-primary px-0.5 font-mono text-[8px] leading-none text-primary-foreground"
          >
            {{ activeFilterCount > 9 ? '9+' : activeFilterCount }}
          </span>
        </Button>

        <Button
          variant="ghost"
          size="icon"
          class="h-9 w-11 shrink-0 rounded-full text-foreground transition-colors hover:bg-black/10 active:bg-black/15 dark:hover:bg-white/10 dark:active:bg-white/15"
          :aria-label="t('gallery.mobile.toolbar.viewSettings.title')"
          @click="openViewSettingsPanel"
        >
          <SlidersHorizontal class="size-4.5" :stroke-width="1.5" />
        </Button>
      </div>
    </template>
  </div>

  <GalleryFilterSheet :open="filterOpen" @update:open="handleFilterOpenChange" />
  <GalleryViewSettingsSheet :open="viewSettingsOpen" @update:open="handleViewSettingsOpenChange" />
</template>
