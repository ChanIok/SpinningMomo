<script setup lang="ts">
import { computed } from 'vue'
import { ArrowUpDown, LayoutGrid } from '@lucide/vue'
import {
  DropdownMenuRadioGroup,
  DropdownMenuRadioItem,
  DropdownMenuSeparator,
  DropdownMenuSub,
  DropdownMenuSubContent,
  DropdownMenuSubTrigger,
} from '@/components/ui/dropdown-menu'
import { useI18n } from '@/composables/useI18n'
import { useGalleryStore } from '../../store'
import { useGalleryViewControls } from '../../composables'
import type { SortBy, SortOrder, ViewMode } from '../../types'
import GalleryPasteDropdownMenuItem from './GalleryPasteDropdownMenuItem.vue'
import GallerySelectionDropdownMenu from './GallerySelectionDropdownMenu.vue'

const { t } = useI18n()
const store = useGalleryStore()
const { setViewMode, sortOptions } = useGalleryViewControls()
const viewMode = computed(() => store.view.mode)
const sortBy = computed(() => store.sortBy)
const sortOrder = computed(() => store.sortOrder)

const viewModeOptions = [
  { value: 'grid' as ViewMode, i18nKey: 'gallery.toolbar.viewMode.grid' },
  { value: 'adaptive' as ViewMode, i18nKey: 'gallery.toolbar.viewMode.adaptive' },
  { value: 'masonry' as ViewMode, i18nKey: 'gallery.toolbar.viewMode.masonry' },
  { value: 'list' as ViewMode, i18nKey: 'gallery.toolbar.viewMode.list' },
]

function setSortBy(value: SortBy) {
  store.setSorting(value, sortOrder.value)
}

function setSortOrder(value: SortOrder) {
  store.setSorting(sortBy.value, value)
}
</script>

<template>
  <GallerySelectionDropdownMenu />
  <DropdownMenuSeparator />
  <GalleryPasteDropdownMenuItem />
  <DropdownMenuSeparator />

  <DropdownMenuSub>
    <DropdownMenuSubTrigger>
      <LayoutGrid />
      {{ t('gallery.toolbar.viewMode.label') }}
    </DropdownMenuSubTrigger>
    <DropdownMenuSubContent class="w-44">
      <DropdownMenuRadioGroup :model-value="viewMode">
        <DropdownMenuRadioItem
          v-for="option in viewModeOptions"
          :key="option.value"
          :value="option.value"
          @click="setViewMode(option.value)"
        >
          {{ t(option.i18nKey) }}
        </DropdownMenuRadioItem>
      </DropdownMenuRadioGroup>
    </DropdownMenuSubContent>
  </DropdownMenuSub>

  <DropdownMenuSub>
    <DropdownMenuSubTrigger>
      <ArrowUpDown />
      {{ t('gallery.toolbar.sort.label') }}
    </DropdownMenuSubTrigger>
    <DropdownMenuSubContent class="w-44">
      <DropdownMenuRadioGroup :model-value="sortBy">
        <DropdownMenuRadioItem
          v-for="option in sortOptions"
          :key="option.value"
          :value="option.value"
          @click="setSortBy(option.value)"
        >
          {{ t(option.i18nKey) }}
        </DropdownMenuRadioItem>
      </DropdownMenuRadioGroup>
      <DropdownMenuSeparator />
      <DropdownMenuRadioGroup :model-value="sortOrder">
        <DropdownMenuRadioItem value="asc" @click="setSortOrder('asc')">
          {{ t('gallery.toolbar.sortOrder.asc') }}
        </DropdownMenuRadioItem>
        <DropdownMenuRadioItem value="desc" @click="setSortOrder('desc')">
          {{ t('gallery.toolbar.sortOrder.desc') }}
        </DropdownMenuRadioItem>
      </DropdownMenuRadioGroup>
    </DropdownMenuSubContent>
  </DropdownMenuSub>
</template>
