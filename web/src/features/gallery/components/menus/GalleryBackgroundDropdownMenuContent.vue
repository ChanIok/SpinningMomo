<script setup lang="ts">
import { ArrowUpDown, LayoutGrid } from 'lucide-vue-next'
import {
  DropdownMenuRadioGroup,
  DropdownMenuRadioItem,
  DropdownMenuSeparator,
  DropdownMenuSub,
  DropdownMenuSubContent,
  DropdownMenuSubTrigger,
} from '@/components/ui/dropdown-menu'
import { useI18n } from '@/composables/useI18n'
import { useGalleryView } from '../../composables'
import type { SortBy, SortOrder, ViewMode } from '../../types'
import GalleryPasteDropdownMenuItem from './GalleryPasteDropdownMenuItem.vue'

const { t } = useI18n()
const galleryView = useGalleryView()
const viewMode = galleryView.viewMode
const sortBy = galleryView.sortBy
const sortOrder = galleryView.sortOrder

const viewModeOptions = [
  { value: 'grid' as ViewMode, i18nKey: 'gallery.toolbar.viewMode.grid' },
  { value: 'adaptive' as ViewMode, i18nKey: 'gallery.toolbar.viewMode.adaptive' },
  { value: 'masonry' as ViewMode, i18nKey: 'gallery.toolbar.viewMode.masonry' },
  { value: 'list' as ViewMode, i18nKey: 'gallery.toolbar.viewMode.list' },
]

const sortByOptions = [
  { value: 'createdAt' as SortBy, i18nKey: 'gallery.toolbar.sort.createdAt' },
  { value: 'name' as SortBy, i18nKey: 'gallery.toolbar.sort.name' },
  { value: 'resolution' as SortBy, i18nKey: 'gallery.toolbar.sort.resolution' },
  { value: 'size' as SortBy, i18nKey: 'gallery.toolbar.sort.size' },
]

function setSortBy(value: SortBy) {
  galleryView.setSorting(value, sortOrder.value)
}

function setSortOrder(value: SortOrder) {
  galleryView.setSorting(sortBy.value, value)
}
</script>

<template>
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
          @click="galleryView.setViewMode(option.value)"
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
          v-for="option in sortByOptions"
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
