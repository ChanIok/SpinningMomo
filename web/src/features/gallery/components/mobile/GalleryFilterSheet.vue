<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { CalendarClock, ChevronDown, Image, Palette, Search, Star, Video, X } from '@lucide/vue'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { MobileDrawer } from '@/components/ui/mobile-drawer'
import { RangeCalendar } from '@/components/ui/range-calendar'
import { ScrollArea } from '@/components/ui/scroll-area'
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
    ? `${activeColorHex.value} (±${activeColorDistance.value})`
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

interface MediaTypeOption {
  value: string
  label: string
  icon?: typeof Image
}

interface ReviewFlagOption {
  value: string
  label: string
  activeClass: string
}

const mediaTypeOptions = computed<MediaTypeOption[]>(() => [
  { value: 'all', label: t('gallery.toolbar.filter.type.all') },
  { value: 'photo', label: t('gallery.toolbar.filter.type.photo'), icon: Image },
  { value: 'video', label: t('gallery.toolbar.filter.type.video'), icon: Video },
])

const reviewFlagOptions = computed<ReviewFlagOption[]>(() => [
  {
    value: 'all',
    label: t('gallery.toolbar.filter.flag.all'),
    activeClass: 'bg-sidebar-accent font-medium text-primary shadow-xs',
  },
  {
    value: 'picked',
    label: t('gallery.toolbar.filter.flag.picked'),
    activeClass: 'bg-amber-500/15 font-medium text-amber-500 dark:text-amber-400 shadow-xs',
  },
  {
    value: 'rejected',
    label: t('gallery.toolbar.filter.flag.rejected'),
    activeClass: 'bg-rose-500/15 font-medium text-rose-500 dark:text-rose-400 shadow-xs',
  },
  {
    value: 'none',
    label: t('gallery.toolbar.filter.flag.none'),
    activeClass: 'bg-sidebar-accent font-medium text-primary shadow-xs',
  },
])
</script>

<template>
  <MobileDrawer
    :open="open"
    side="right"
    class="flex h-full w-[88vw] max-w-[360px] flex-col gap-0 border-l border-border/40 bg-background text-sidebar-foreground shadow-2xl"
    @close="close"
  >
    <!-- 抽屉头部 -->
    <header
      class="flex h-12 shrink-0 items-center justify-between border-b border-border/40 px-4 pt-[env(safe-area-inset-top)]"
    >
      <span class="text-sm font-medium text-foreground">
        {{ t('gallery.mobile.toolbar.filter.title') }}
      </span>
      <div class="flex items-center gap-1">
        <Button
          v-if="hasAttributeFilters"
          variant="ghost"
          size="sm"
          class="h-7 px-2 text-xs font-normal text-primary hover:bg-sidebar-hover hover:text-primary"
          @click="clearAttributeFilters"
        >
          {{ t('gallery.toolbar.filters.clear') }}
        </Button>
        <Button
          variant="ghost"
          size="icon"
          class="h-7 w-7 rounded-md text-sidebar-foreground/80 hover:bg-sidebar-hover hover:text-sidebar-accent-foreground"
          :aria-label="t('common.close')"
          @click="close"
        >
          <X class="size-4" />
        </Button>
      </div>
    </header>

    <!-- 滚动内容区 -->
    <ScrollArea class="min-h-0 flex-1">
      <div class="space-y-5 px-4 py-4 pb-[calc(env(safe-area-inset-bottom)+1.5rem)]">
        <!-- 关键词搜索 -->
        <div class="space-y-2">
          <label class="text-sm font-medium text-foreground">
            {{ t('gallery.toolbar.filters.keyword') }}
          </label>
          <div class="relative">
            <Search
              class="absolute top-1/2 left-2.5 size-3.5 -translate-y-1/2 text-muted-foreground"
            />
            <Input
              :model-value="searchQuery"
              :placeholder="t('gallery.toolbar.search.placeholder')"
              class="h-9 rounded-md border-border/40 bg-sidebar-hover/60 pr-8 pl-8 text-xs text-sidebar-foreground placeholder:text-muted-foreground focus-visible:border-ring focus-visible:bg-sidebar-accent/40"
              @update:model-value="updateSearchQuery"
            />
            <button
              v-if="searchQuery"
              type="button"
              class="absolute top-1/2 right-2 -translate-y-1/2 rounded p-0.5 text-muted-foreground transition-colors hover:text-sidebar-foreground"
              :aria-label="t('gallery.toolbar.filters.clear')"
              @click="clearSearch"
            >
              <X class="size-3.5" />
            </button>
          </div>
        </div>

        <!-- 媒体类型（胶囊分段控制器） -->
        <div class="space-y-2">
          <label class="text-sm font-medium text-foreground">
            {{ t('gallery.toolbar.filters.fileType') }}
          </label>
          <div
            class="grid grid-cols-3 gap-1 rounded-lg border border-border/30 bg-sidebar-hover/50 p-1"
          >
            <button
              v-for="opt in mediaTypeOptions"
              :key="opt.value"
              type="button"
              class="flex h-8.5 items-center justify-center gap-1.5 rounded-md text-xs font-medium transition-colors duration-150"
              :class="
                (filter.type || 'all') === opt.value
                  ? 'bg-sidebar-accent font-medium text-primary shadow-xs [&_svg]:text-primary'
                  : 'text-sidebar-foreground hover:bg-sidebar-hover hover:text-sidebar-accent-foreground'
              "
              @click="setType(opt.value)"
            >
              <component :is="opt.icon" v-if="opt.icon" class="size-3.5" />
              <span>{{ opt.label }}</span>
            </button>
          </div>
        </div>

        <!-- 标记状态（语义化分段胶囊） -->
        <div class="space-y-2">
          <label class="text-sm font-medium text-foreground">
            {{ t('gallery.toolbar.filters.reviewFlag') }}
          </label>
          <div
            class="grid grid-cols-4 gap-1 rounded-lg border border-border/30 bg-sidebar-hover/50 p-1"
          >
            <button
              v-for="opt in reviewFlagOptions"
              :key="opt.value"
              type="button"
              class="flex h-8.5 items-center justify-center rounded-md text-xs font-medium transition-colors duration-150"
              :class="[
                (filter.reviewFlag || 'all') === opt.value
                  ? opt.activeClass
                  : 'text-sidebar-foreground hover:bg-sidebar-hover hover:text-sidebar-accent-foreground',
              ]"
              @click="setReviewFlag(opt.value)"
            >
              <span>{{ opt.label }}</span>
            </button>
          </div>
        </div>

        <!-- 评分筛选（精简 6 联星级胶囊） -->
        <div class="space-y-2">
          <div class="flex items-center justify-between">
            <label class="text-sm font-medium text-foreground">
              {{ t('gallery.toolbar.filter.rating.label') }}
            </label>
            <button
              v-if="selectedRatings.length > 0"
              type="button"
              class="text-xs text-muted-foreground transition-colors hover:text-sidebar-foreground"
              @click="clearRatingFilter"
            >
              {{ t('gallery.toolbar.filter.rating.clear') }}
            </button>
          </div>
          <div
            class="grid grid-cols-6 gap-1 rounded-lg border border-border/30 bg-sidebar-hover/50 p-1"
          >
            <button
              v-for="rating in [5, 4, 3, 2, 1, 0]"
              :key="rating"
              type="button"
              class="flex h-8.5 items-center justify-center gap-1 rounded-md text-xs font-medium transition-colors duration-150"
              :class="
                isRatingSelected(rating)
                  ? 'bg-sidebar-accent font-medium text-primary shadow-xs'
                  : 'text-sidebar-foreground hover:bg-sidebar-hover hover:text-sidebar-accent-foreground'
              "
              @click="toggleRatingFilter(rating)"
            >
              <template v-if="rating === 0">
                <span>{{ t('gallery.toolbar.filter.rating.unrated') }}</span>
              </template>
              <template v-else>
                <Star
                  class="size-3.5"
                  :class="
                    isRatingSelected(rating)
                      ? 'fill-amber-400 text-amber-400'
                      : 'text-muted-foreground/40'
                  "
                />
                <span class="font-mono">{{ rating }}</span>
              </template>
            </button>
          </div>
        </div>

        <!-- 日期范围（轻量单行折叠卡片） -->
        <div
          class="rounded-lg border border-border/40 bg-sidebar-hover/30 p-3 transition-colors duration-150 hover:bg-sidebar-hover/50"
        >
          <button
            type="button"
            class="flex w-full items-center justify-between text-left"
            @click="toggleDateSection"
          >
            <div class="flex min-w-0 items-center gap-2">
              <CalendarClock class="size-4 shrink-0 text-muted-foreground" />
              <span class="text-sm font-medium text-foreground">
                {{ t('gallery.toolbar.dateFilter.title') }}
              </span>
            </div>
            <div class="flex min-w-0 items-center gap-1.5">
              <span
                class="truncate text-xs"
                :class="hasActiveDateFilter ? 'font-medium text-primary' : 'text-muted-foreground'"
              >
                {{ dateFilterSummary }}
              </span>
              <ChevronDown
                class="size-3.5 text-muted-foreground transition-transform duration-200"
                :class="dateExpanded ? 'rotate-180' : ''"
              />
            </div>
          </button>

          <div v-if="dateExpanded" class="mt-2.5 space-y-2.5 border-t border-border/40 pt-2.5">
            <RangeCalendar
              :model-value="draftDateRange"
              :locale="locale"
              class="mx-auto w-fit origin-top scale-95 p-0"
              @update:model-value="onDateRangeChange"
            />
            <div class="flex justify-end gap-1.5 pt-1">
              <Button
                v-if="hasActiveDateFilter"
                variant="ghost"
                size="sm"
                class="h-7.5 px-2 text-xs text-muted-foreground hover:text-sidebar-foreground"
                @click="clearDateFilter"
              >
                {{ t('gallery.toolbar.dateFilter.clear') }}
              </Button>
              <Button size="sm" class="h-7.5 px-3 text-xs" @click="applyDateFilter">
                {{ t('gallery.toolbar.dateFilter.apply') }}
              </Button>
            </div>
          </div>
        </div>

        <!-- 色彩筛选（轻量单行折叠卡片） -->
        <div
          class="rounded-lg border border-border/40 bg-sidebar-hover/30 p-3 transition-colors duration-150 hover:bg-sidebar-hover/50"
        >
          <button
            type="button"
            class="flex w-full items-center justify-between text-left"
            @click="toggleColorSection"
          >
            <div class="flex min-w-0 items-center gap-2">
              <Palette class="size-4 shrink-0 text-muted-foreground" />
              <span class="text-sm font-medium text-foreground">
                {{ t('gallery.toolbar.colorFilter.title') }}
              </span>
            </div>
            <div class="flex min-w-0 items-center gap-1.5">
              <span
                v-if="activeColorHex"
                class="size-3 shrink-0 rounded-full border border-border/80 shadow-xs"
                :style="{ backgroundColor: activeColorHex }"
              />
              <span
                class="truncate text-xs"
                :class="activeColorHex ? 'font-medium text-primary' : 'text-muted-foreground'"
              >
                {{ colorFilterSummary }}
              </span>
              <ChevronDown
                class="size-3.5 text-muted-foreground transition-transform duration-200"
                :class="colorExpanded ? 'rotate-180' : ''"
              />
            </div>
          </button>

          <div v-if="colorExpanded" class="mt-2.5 space-y-3 border-t border-border/40 pt-2.5">
            <ColorPicker
              :model-value="draftColorHex"
              @update:model-value="(color) => (draftColorHex = color)"
            />
            <div class="space-y-1.5 px-0.5">
              <div class="flex items-center justify-between text-xs text-sidebar-foreground">
                <span>{{ t('gallery.toolbar.colorFilter.distance.label') }}</span>
                <span class="font-mono font-medium text-primary">{{ draftColorDistance }}</span>
              </div>
              <Slider
                :model-value="[draftColorDistance]"
                :min="COLOR_DISTANCE_MIN"
                :max="COLOR_DISTANCE_MAX"
                :step="1"
                @update:model-value="onColorDistanceChange"
              />
            </div>
            <div class="flex justify-end gap-1.5 pt-1">
              <Button
                v-if="activeColorHex"
                variant="ghost"
                size="sm"
                class="h-7.5 px-2 text-xs text-muted-foreground hover:text-sidebar-foreground"
                @click="clearColorFilter"
              >
                {{ t('gallery.toolbar.colorFilter.clear') }}
              </Button>
              <Button size="sm" class="h-7.5 px-3 text-xs" @click="applyColorFilter">
                {{ t('gallery.toolbar.colorFilter.apply') }}
              </Button>
            </div>
          </div>
        </div>
      </div>
    </ScrollArea>
  </MobileDrawer>
</template>
