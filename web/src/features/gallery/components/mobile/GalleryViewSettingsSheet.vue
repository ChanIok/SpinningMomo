<script setup lang="ts">
import { ArrowUpDown, Check, Folder } from '@lucide/vue'
import { Button } from '@/components/ui/button'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { Sheet, SheetContent, SheetHeader, SheetTitle } from '@/components/ui/sheet'
import { Slider } from '@/components/ui/slider'
import { Toggle } from '@/components/ui/toggle'
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
</script>

<template>
  <Sheet :open="open" @update:open="emit('update:open', $event)">
    <SheetContent
      side="bottom"
      class="max-h-[88vh] rounded-t-2xl px-4 pb-[calc(env(safe-area-inset-bottom)+1rem)]"
    >
      <SheetHeader class="px-0">
        <SheetTitle>{{ t('gallery.mobile.toolbar.viewSettings.title') }}</SheetTitle>
      </SheetHeader>

      <div class="min-h-0 flex-1 space-y-6 overflow-y-auto pb-2">
        <section class="space-y-3">
          <h3 class="text-sm font-medium">{{ t('gallery.toolbar.folderOptions.label') }}</h3>
          <Toggle
            size="lg"
            class="h-12 w-full justify-start gap-3 px-3"
            :model-value="currentFolderOnly"
            @update:model-value="onCurrentFolderOnlyChange"
          >
            <Folder class="size-5" />
            {{ t('gallery.toolbar.folderOptions.currentFolderOnly') }}
          </Toggle>
        </section>

        <section class="space-y-3 border-t border-border/60 pt-5">
          <h3 class="text-sm font-medium">{{ t('gallery.toolbar.sort.label') }}</h3>
          <div class="flex gap-2">
            <Select :model-value="sortBy" @update:model-value="onSortByChange">
              <SelectTrigger class="h-11 min-w-0 flex-1">
                <SelectValue />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="createdAt">{{ t('gallery.toolbar.sort.createdAt') }}</SelectItem>
                <SelectItem value="name">{{ t('gallery.toolbar.sort.name') }}</SelectItem>
                <SelectItem value="resolution">{{
                  t('gallery.toolbar.sort.resolution')
                }}</SelectItem>
                <SelectItem value="size">{{ t('gallery.toolbar.sort.size') }}</SelectItem>
              </SelectContent>
            </Select>
            <Button variant="outline" class="h-11 shrink-0" @click="toggleSortOrder">
              <ArrowUpDown class="mr-1.5 size-4" />
              {{
                sortOrder === 'asc'
                  ? t('gallery.toolbar.sortOrder.asc')
                  : t('gallery.toolbar.sortOrder.desc')
              }}
            </Button>
          </div>
        </section>

        <section class="space-y-3 border-t border-border/60 pt-5">
          <h3 class="text-sm font-medium">{{ t('gallery.toolbar.viewMode.label') }}</h3>
          <div class="grid grid-cols-3 gap-2">
            <Button
              v-for="mode in availableViewModes"
              :key="mode.value"
              variant="outline"
              class="h-16 flex-col gap-1.5"
              :class="viewMode === mode.value ? 'border-primary text-primary' : ''"
              @click="setViewMode(mode.value)"
            >
              <component :is="mode.icon" class="size-5" />
              <span class="text-xs">{{ t(mode.i18nKey) }}</span>
            </Button>
          </div>
        </section>

        <section class="space-y-3 border-t border-border/60 pt-5">
          <div class="flex items-center justify-between">
            <h3 class="text-sm font-medium">{{ t('gallery.toolbar.thumbnailSize.label') }}</h3>
            <span class="text-xs text-muted-foreground">{{ currentSliderPosition }}%</span>
          </div>
          <Slider
            :model-value="[currentSliderPosition]"
            :min="0"
            :max="100"
            :step="1"
            @update:model-value="onViewSizeSliderChange"
          />
          <div class="flex justify-between text-xs text-muted-foreground">
            <span>{{ t('gallery.toolbar.thumbnailSize.fine') }}</span>
            <span>{{ t('gallery.toolbar.thumbnailSize.showcase') }}</span>
          </div>
        </section>
      </div>

      <div class="flex justify-end pt-3">
        <Button class="min-w-24" @click="emit('update:open', false)">
          <Check class="mr-1.5 size-4" />
          {{ t('common.done') }}
        </Button>
      </div>
    </SheetContent>
  </Sheet>
</template>
