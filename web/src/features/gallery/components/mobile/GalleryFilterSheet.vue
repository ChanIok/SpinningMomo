<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import {
  CalendarClock,
  Check,
  ChevronDown,
  Flag,
  Image,
  Palette,
  Star,
  Video,
  X,
} from '@lucide/vue'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { RangeCalendar } from '@/components/ui/range-calendar'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Sheet, SheetContent, SheetDescription, SheetTitle } from '@/components/ui/sheet'
import { Slider } from '@/components/ui/slider'
import ColorPicker from '@/components/ui/color-picker/ColorPicker.vue'
import { useI18n } from '@/composables/useI18n'
import { useGalleryFilterControls } from '../../composables'

const props = defineProps<{ open: boolean }>()
const emit = defineEmits<{ 'update:open': [value: boolean] }>()

const { t, locale } = useI18n()
const {
  filter,
  searchQuery,
  activeColorHex,
  activeColorDistance,
  selectedRatings,
  typeFilterLabel,
  ratingFilterLabel,
  reviewFlagFilterLabel,
  draftColorHex,
  draftColorDistance,
  draftDateRange,
  displayDateFilterLabel,
  hasAttributeFilters,
  onDateRangeChange,
  applyDateFilter,
  clearDateFilter,
  updateSearchQuery,
  clearSearch,
  onTypeFilterChange,
  onReviewFlagChange,
  isRatingSelected,
  toggleRatingFilter,
  clearRatingFilter,
  applyColorFilter,
  clearColorFilter,
  onColorDistanceChange,
  clearAttributeFilters,
  syncFilterDrafts,
  COLOR_DISTANCE_MIN,
  COLOR_DISTANCE_MAX,
} = useGalleryFilterControls()

const dateExpanded = ref(false)
const colorExpanded = ref(false)
const hasActiveDateFilter = computed(
  () => filter.value.createdAtFrom !== undefined || filter.value.createdAtTo !== undefined
)
const dateFilterSummary = computed(() =>
  hasActiveDateFilter.value ? displayDateFilterLabel.value : t('gallery.toolbar.dateFilter.none')
)
const colorFilterSummary = computed(() =>
  activeColorHex.value
    ? `${activeColorHex.value} · ${activeColorDistance.value}`
    : t('gallery.toolbar.colorFilter.none')
)

watch(
  () => props.open,
  (open) => {
    if (open) {
      syncFilterDrafts()
      dateExpanded.value = false
      colorExpanded.value = false
    }
  }
)

function close() {
  emit('update:open', false)
}

function setType(value: string) {
  onTypeFilterChange(value)
}

function setReviewFlag(value: string) {
  onReviewFlagChange(value)
}

function toggleDateSection() {
  dateExpanded.value = !dateExpanded.value
}

function toggleColorSection() {
  colorExpanded.value = !colorExpanded.value
}
</script>

<template>
  <Sheet :open="open" @update:open="emit('update:open', $event)">
    <SheetContent
      side="right"
      class="flex h-full max-h-none w-full max-w-none flex-col gap-0 rounded-none border-0 p-0 sm:max-w-none"
    >
      <SheetTitle class="sr-only">{{ t('gallery.mobile.toolbar.filter.title') }}</SheetTitle>
      <SheetDescription class="sr-only">
        {{ t('gallery.mobile.toolbar.filter.description') }}
      </SheetDescription>

      <header
        class="flex shrink-0 items-center justify-between border-b border-border/60 px-4 pt-[env(safe-area-inset-top)] pr-14"
      >
        <h2 class="text-sm font-semibold">{{ t('gallery.mobile.toolbar.filter.title') }}</h2>
        <Button
          variant="ghost"
          size="sm"
          class="h-10 px-3 text-sm"
          :class="hasAttributeFilters ? 'text-primary' : ''"
          @click="clearAttributeFilters"
        >
          {{ t('gallery.toolbar.filters.clear') }}
        </Button>
      </header>

      <ScrollArea class="min-h-0 flex-1">
        <div class="mx-auto w-full max-w-2xl space-y-6 px-4 py-5 pb-8">
          <section class="space-y-2">
            <h3 class="text-sm font-medium">{{ t('gallery.toolbar.filters.keyword') }}</h3>
            <div class="relative">
              <Input
                :model-value="searchQuery"
                :placeholder="t('gallery.toolbar.search.placeholder')"
                class="h-11 pr-10"
                @update:model-value="updateSearchQuery"
              />
              <Button
                v-if="searchQuery"
                variant="ghost"
                size="icon-xs"
                class="absolute top-1/2 right-2 -translate-y-1/2"
                :aria-label="t('gallery.toolbar.filters.clear')"
                @click="clearSearch"
              >
                <X class="size-4" />
              </Button>
            </div>
          </section>

          <section class="space-y-3 border-t border-border/60 pt-5">
            <button
              id="gallery-filter-date-trigger"
              type="button"
              class="flex w-full items-center justify-between gap-3 rounded-lg border border-border/70 p-3 text-left transition-colors hover:bg-muted/50"
              :aria-expanded="dateExpanded"
              aria-controls="gallery-filter-date-content"
              @click="toggleDateSection"
            >
              <span class="flex min-w-0 items-center gap-3">
                <CalendarClock class="size-5 shrink-0 text-muted-foreground" />
                <span class="min-w-0">
                  <span class="block text-sm font-medium">
                    {{ t('gallery.toolbar.dateFilter.title') }}
                  </span>
                  <span class="block truncate text-xs text-muted-foreground">
                    {{ dateFilterSummary }}
                  </span>
                </span>
              </span>
              <ChevronDown
                class="size-4 shrink-0 text-muted-foreground transition-transform"
                :class="dateExpanded && 'rotate-180'"
              />
            </button>

            <div v-if="dateExpanded" id="gallery-filter-date-content" class="space-y-3 pt-1">
              <RangeCalendar
                :model-value="draftDateRange"
                :locale="locale"
                class="mx-auto w-fit p-0"
                @update:model-value="onDateRangeChange"
              />
              <div class="flex justify-end gap-2">
                <Button variant="outline" size="sm" @click="clearDateFilter">
                  {{ t('gallery.toolbar.dateFilter.clear') }}
                </Button>
                <Button size="sm" @click="applyDateFilter">
                  {{ t('gallery.toolbar.dateFilter.apply') }}
                </Button>
              </div>
            </div>
          </section>

          <section class="space-y-3 border-t border-border/60 pt-5">
            <button
              id="gallery-filter-color-trigger"
              type="button"
              class="flex w-full items-center justify-between gap-3 rounded-lg border border-border/70 p-3 text-left transition-colors hover:bg-muted/50"
              :aria-expanded="colorExpanded"
              aria-controls="gallery-filter-color-content"
              @click="toggleColorSection"
            >
              <span class="flex min-w-0 items-center gap-3">
                <Palette class="size-5 shrink-0 text-muted-foreground" />
                <span class="min-w-0">
                  <span class="block text-sm font-medium">
                    {{ t('gallery.toolbar.colorFilter.title') }}
                  </span>
                  <span
                    class="flex min-w-0 items-center gap-1.5 truncate text-xs text-muted-foreground"
                  >
                    <span
                      v-if="activeColorHex"
                      class="size-3 shrink-0 rounded-full border border-border/80"
                      :style="{ backgroundColor: activeColorHex }"
                    />
                    <span class="truncate">{{ colorFilterSummary }}</span>
                  </span>
                </span>
              </span>
              <ChevronDown
                class="size-4 shrink-0 text-muted-foreground transition-transform"
                :class="colorExpanded && 'rotate-180'"
              />
            </button>

            <div v-if="colorExpanded" id="gallery-filter-color-content" class="space-y-3 pt-1">
              <ColorPicker
                :model-value="draftColorHex"
                @update:model-value="(color) => (draftColorHex = color)"
              />
              <div class="space-y-2">
                <div class="flex items-center justify-between text-xs">
                  <span>{{ t('gallery.toolbar.colorFilter.distance.label') }}</span>
                  <span class="font-mono">{{ draftColorDistance }}</span>
                </div>
                <Slider
                  :model-value="[draftColorDistance]"
                  :min="COLOR_DISTANCE_MIN"
                  :max="COLOR_DISTANCE_MAX"
                  :step="1"
                  @update:model-value="onColorDistanceChange"
                />
              </div>
              <div class="flex justify-end gap-2">
                <Button v-if="activeColorHex" variant="outline" size="sm" @click="clearColorFilter">
                  {{ t('gallery.toolbar.colorFilter.clear') }}
                </Button>
                <Button size="sm" @click="applyColorFilter">
                  {{ t('gallery.toolbar.colorFilter.apply') }}
                </Button>
              </div>
            </div>
          </section>

          <section class="space-y-3 border-t border-border/60 pt-5">
            <div class="flex items-center gap-2">
              <Image class="size-4 text-muted-foreground" />
              <h3 class="text-sm font-medium">{{ t('gallery.toolbar.filters.fileType') }}</h3>
            </div>
            <div class="grid grid-cols-3 gap-2">
              <Button
                v-for="option in [
                  { value: 'all', label: t('gallery.toolbar.filter.type.all') },
                  { value: 'photo', label: t('gallery.toolbar.filter.type.photo') },
                  { value: 'video', label: t('gallery.toolbar.filter.type.video') },
                ]"
                :key="option.value"
                variant="outline"
                class="h-11"
                :class="
                  (filter.type || 'all') === option.value ? 'border-primary text-primary' : ''
                "
                @click="setType(option.value)"
              >
                <Video v-if="option.value === 'video'" class="mr-1.5 size-4" />
                <Image v-else-if="option.value === 'photo'" class="mr-1.5 size-4" />
                {{ option.label }}
              </Button>
            </div>
            <p class="text-xs text-muted-foreground">{{ typeFilterLabel }}</p>
          </section>

          <section class="space-y-3 border-t border-border/60 pt-5">
            <div class="flex items-center gap-2">
              <Star class="size-4 text-muted-foreground" />
              <h3 class="text-sm font-medium">{{ t('gallery.toolbar.filter.rating.label') }}</h3>
            </div>
            <div class="grid grid-cols-3 gap-2 sm:grid-cols-6">
              <Button
                v-for="rating in [5, 4, 3, 2, 1, 0]"
                :key="rating"
                variant="outline"
                class="h-11"
                :class="isRatingSelected(rating) ? 'border-primary text-primary' : ''"
                @click="toggleRatingFilter(rating)"
              >
                <template v-if="rating === 0">
                  <X class="mr-1 size-4" />
                  {{ t('gallery.toolbar.filter.rating.unrated') }}
                </template>
                <template v-else>
                  <Star class="mr-1 size-4 fill-amber-400 text-amber-400" />
                  {{ rating }}
                </template>
              </Button>
            </div>
            <div class="flex items-center justify-between gap-2 text-xs text-muted-foreground">
              <span>{{ ratingFilterLabel }}</span>
              <Button
                v-if="selectedRatings.length > 0"
                variant="ghost"
                size="sm"
                class="h-8 px-2"
                @click="clearRatingFilter"
              >
                {{ t('gallery.toolbar.filter.rating.clear') }}
              </Button>
            </div>
          </section>

          <section class="space-y-3 border-t border-border/60 pt-5">
            <div class="flex items-center gap-2">
              <Flag class="size-4 text-muted-foreground" />
              <h3 class="text-sm font-medium">{{ t('gallery.toolbar.filters.reviewFlag') }}</h3>
            </div>
            <div class="grid grid-cols-2 gap-2">
              <Button
                v-for="option in [
                  { value: 'all', label: t('gallery.toolbar.filter.flag.all') },
                  { value: 'rejected', label: t('gallery.toolbar.filter.flag.rejected') },
                  { value: 'none', label: t('gallery.toolbar.filter.flag.none') },
                  { value: 'picked', label: t('gallery.toolbar.filter.flag.picked') },
                ]"
                :key="option.value"
                variant="outline"
                class="h-11"
                :class="
                  (filter.reviewFlag || 'all') === option.value ? 'border-primary text-primary' : ''
                "
                @click="setReviewFlag(option.value)"
              >
                {{ option.label }}
              </Button>
            </div>
            <p class="text-xs text-muted-foreground">{{ reviewFlagFilterLabel }}</p>
          </section>
        </div>
      </ScrollArea>

      <footer
        class="flex shrink-0 justify-end border-t border-border/60 bg-background/95 px-4 pt-3 pb-[calc(env(safe-area-inset-bottom)+0.75rem)] backdrop-blur"
      >
        <Button class="min-w-24" @click="close">
          <Check class="mr-1.5 size-4" />
          {{ t('common.done') }}
        </Button>
      </footer>
    </SheetContent>
  </Sheet>
</template>
