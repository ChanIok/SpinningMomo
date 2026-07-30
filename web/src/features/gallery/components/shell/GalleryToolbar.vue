<script setup lang="ts">
import { computed, ref, shallowRef, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useElementSize } from '@vueuse/core'
import { Button } from '@/components/ui/button'
import { Checkbox } from '@/components/ui/checkbox'
import { Input } from '@/components/ui/input'
import { Slider } from '@/components/ui/slider'
import { Toggle } from '@/components/ui/toggle'
import { Popover, PopoverContent, PopoverTrigger } from '@/components/ui/popover'
import { RangeCalendar } from '@/components/ui/range-calendar'
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from '@/components/ui/tooltip'
import { ScrollArea, ScrollBar } from '@/components/ui/scroll-area'
import ColorPicker from '@/components/ui/color-picker/ColorPicker.vue'
import { CalendarDate } from '@internationalized/date'
import type { DateRange, DateValue } from 'reka-ui'
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuLabel,
  DropdownMenuRadioGroup,
  DropdownMenuRadioItem,
  DropdownMenuSeparator,
  DropdownMenuTrigger,
} from '@/components/ui/dropdown-menu'
import {
  ArrowUpDown,
  CalendarClock,
  ChevronDown,
  Flag,
  Folder,
  Grid3x3,
  Image,
  Images,
  LayoutGrid,
  List,
  Map,
  Palette,
  Rows3,
  Search,
  Settings,
  Star,
  Tag,
  Type,
  Video,
  X,
} from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import { useSettingsStore } from '@/features/settings/store'
import { pushWithViewTransition } from '@/router/viewTransition'
import { useGalleryView } from '../../composables'
import { useGalleryStore } from '../../store'
import type {
  AssetFilter,
  AssetType,
  FolderTreeNode,
  ReviewFlag,
  SortBy,
  TagTreeNode,
  ViewMode,
} from '../../types'

type SourceType = 'all' | 'folder' | 'tag'

const emit = defineEmits<{
  'open-preferences': []
}>()

const { t, locale } = useI18n()
const router = useRouter()
const settingsStore = useSettingsStore()
const store = useGalleryStore()
const galleryView = useGalleryView()

const showMapEntry = computed(() => settingsStore.appSettings.extensions.infinityNikki.enable)

const isPreferencesTooltipAllowed = ref(true)

const handleOpenMap = () => {
  void pushWithViewTransition(router, '/map')
}

const handleOpenPreferences = () => {
  isPreferencesTooltipAllowed.value = false
  emit('open-preferences')
}

const viewMode = computed(() => galleryView.viewMode.value)
const sortBy = computed(() => galleryView.sortBy.value)
const sortOrder = computed(() => galleryView.sortOrder.value)
const currentFolderOnly = computed(() => !galleryView.includeSubfolders.value)
const filter = computed(() => galleryView.filter.value)
const searchQuery = computed(() => filter.value.searchQuery || '')
const activeColorHex = computed(() => filter.value.colorHex)
const activeColorDistance = computed(
  () => (filter.value as AssetFilter & { colorDistance?: number }).colorDistance ?? 18
)
const activeDateFrom = computed(() => filter.value.createdAtFrom)
const activeDateTo = computed(() => filter.value.createdAtTo)

const COLOR_DISTANCE_MIN = 1
const COLOR_DISTANCE_MAX = 40
const COLOR_DISTANCE_DEFAULT = 18
const STARS = [1, 2, 3, 4, 5] as const

const currentSliderPosition = computed(() => galleryView.getSliderPosition())
const colorPopoverOpen = ref(false)
const draftColorHex = ref(activeColorHex.value || '#FFFFFF')
const draftColorDistance = ref(activeColorDistance.value)
const datePopoverOpen = ref(false)
const draftDateRange = shallowRef<DateRange>({ start: undefined, end: undefined })

const toolbarRef = ref<HTMLElement | null>(null)
const { width: toolbarWidth } = useElementSize(toolbarRef)
const isWide = computed(() => toolbarWidth.value >= 720)
const isFilterCompact = computed(() => toolbarWidth.value > 0 && toolbarWidth.value < 480)

const filterScrollAreaRef = ref<InstanceType<typeof ScrollArea> | null>(null)

function handleFilterWheel(event: WheelEvent) {
  const el = filterScrollAreaRef.value?.viewportElement
  if (el && el.scrollWidth > el.clientWidth) {
    el.scrollLeft += event.deltaY
  }
}

const hasAttributeFilters = computed(
  () =>
    Boolean(searchQuery.value) ||
    activeDateFrom.value !== undefined ||
    activeDateTo.value !== undefined ||
    filter.value.type !== undefined ||
    selectedRatings.value.length > 0 ||
    filter.value.reviewFlag !== undefined ||
    Boolean(filter.value.colorHex)
)

const viewModes = [
  { value: 'grid' as ViewMode, icon: Grid3x3, i18nKey: 'gallery.toolbar.viewMode.grid' },
  { value: 'adaptive' as ViewMode, icon: Rows3, i18nKey: 'gallery.toolbar.viewMode.adaptive' },
  { value: 'masonry' as ViewMode, icon: LayoutGrid, i18nKey: 'gallery.toolbar.viewMode.masonry' },
  { value: 'list' as ViewMode, icon: List, i18nKey: 'gallery.toolbar.viewMode.list' },
]

const currentViewModeIcon = computed(() => {
  const mode = viewModes.find((m) => m.value === viewMode.value)
  return mode?.icon || Grid3x3
})

const currentSource = computed<{ type: SourceType; label: string }>(() => {
  const folderId = filter.value.folderId ? Number(filter.value.folderId) : null
  if (folderId !== null && Number.isFinite(folderId)) {
    return {
      type: 'folder',
      label:
        findFolderNameById(store.folders, folderId) ||
        t('gallery.toolbar.browse.folderFallback', { id: folderId }),
    }
  }

  const tagId = filter.value.tagIds?.[0]
  if (tagId !== undefined) {
    return {
      type: 'tag',
      label:
        findTagNameById(store.tags, tagId) ||
        t('gallery.toolbar.browse.tagFallback', { id: tagId }),
    }
  }

  return { type: 'all', label: t('gallery.toolbar.browse.all') }
})

const selectedRatings = computed(() => normalizeRatings(filter.value.ratings))
const typeFilterLabel = computed(() => getTypeLabel(filter.value.type))
const ratingFilterLabel = computed(() => getRatingLabel(selectedRatings.value))
const reviewFlagFilterLabel = computed(() => getReviewFlagLabel(filter.value.reviewFlag))
const displayDateRangeMillis = computed(() => {
  const draftStart = draftDateRange.value.start
  if (draftStart) {
    const draftEnd = draftDateRange.value.end ?? draftStart
    const [rangeStart, rangeEnd] = orderRangeDates(draftStart, draftEnd)
    return {
      from: calendarDateToLocalStartMillis(rangeStart),
      to: calendarDateToExclusiveEndMillis(rangeEnd),
    }
  }

  return {
    from: activeDateFrom.value,
    to: activeDateTo.value,
  }
})
const displayDateFilterLabel = computed(() =>
  getDateFilterLabel(displayDateRangeMillis.value.from, displayDateRangeMillis.value.to)
)
const hasDisplayDateRange = computed(
  () =>
    displayDateRangeMillis.value.from !== undefined || displayDateRangeMillis.value.to !== undefined
)
const sortOrderLabel = computed(() =>
  sortOrder.value === 'asc'
    ? t('gallery.toolbar.sortOrder.asc')
    : t('gallery.toolbar.sortOrder.desc')
)

watch(colorPopoverOpen, (open) => {
  if (open) {
    draftColorHex.value = activeColorHex.value || '#FFFFFF'
    draftColorDistance.value = activeColorDistance.value
  }
})

watch(datePopoverOpen, (open) => {
  if (open) {
    draftDateRange.value = {
      start: millisToCalendarDate(activeDateFrom.value),
      end: millisToCalendarDate(activeDateTo.value, true),
    }
  }
})

function findFolderNameById(nodes: FolderTreeNode[], id: number): string | null {
  for (const node of nodes) {
    if (node.id === id) {
      return node.displayName || node.name
    }
    const childName = findFolderNameById(node.children, id)
    if (childName) {
      return childName
    }
  }
  return null
}

function findTagNameById(nodes: TagTreeNode[], id: number): string | null {
  for (const node of nodes) {
    if (node.id === id) {
      return node.name
    }
    const childName = findTagNameById(node.children, id)
    if (childName) {
      return childName
    }
  }
  return null
}

function getTypeLabel(type?: AssetType): string {
  if (type === 'photo') return t('gallery.toolbar.filter.type.photo')
  if (type === 'video') return t('gallery.toolbar.filter.type.video')
  return t('gallery.toolbar.filters.fileType')
}

function normalizeRatings(ratings?: number[]): number[] {
  return [...new Set(ratings ?? [])]
    .filter((rating) => Number.isInteger(rating) && rating >= 0 && rating <= 5)
    .sort((a, b) => b - a)
}

function getRatingLabel(ratings: number[]): string {
  if (ratings.length === 0) return t('gallery.toolbar.filters.rating')
  if (ratings.length === 1) {
    const rating = ratings[0]
    if (rating === 0) return t('gallery.toolbar.filter.rating.unrated')
    return t('gallery.toolbar.filters.ratingValue', { rating })
  }

  const positiveRatings = ratings.filter((rating) => rating > 0)
  const includesUnrated = ratings.includes(0)
  const maxRating = positiveRatings[0]
  const minRating = positiveRatings[positiveRatings.length - 1]
  const isContinuous =
    !includesUnrated &&
    positiveRatings.length === ratings.length &&
    maxRating !== undefined &&
    minRating !== undefined &&
    maxRating - minRating === positiveRatings.length - 1

  if (isContinuous) {
    return t('gallery.toolbar.filters.ratingRange', {
      min: minRating,
      max: maxRating,
    })
  }

  return t('gallery.toolbar.filters.ratingCount', { count: ratings.length })
}

function getReviewFlagLabel(reviewFlag?: ReviewFlag): string {
  if (reviewFlag === 'none') return t('gallery.toolbar.filter.flag.none')
  if (reviewFlag === 'picked') return t('gallery.toolbar.filter.flag.picked')
  if (reviewFlag === 'rejected') return t('gallery.toolbar.filter.flag.rejected')
  return t('gallery.toolbar.filters.reviewFlag')
}

function millisToCalendarDate(value?: number, exclusiveEnd = false): DateValue | undefined {
  if (value === undefined) return undefined

  const date = new Date(exclusiveEnd ? value - 1 : value)
  if (!Number.isFinite(date.getTime())) return undefined

  return new CalendarDate(date.getFullYear(), date.getMonth() + 1, date.getDate())
}

function calendarDateToLocalStartMillis(date: DateValue): number {
  return new Date(date.year, date.month - 1, date.day).getTime()
}

function calendarDateToExclusiveEndMillis(date: DateValue): number {
  return new Date(date.year, date.month - 1, date.day + 1).getTime()
}

function orderRangeDates(start: DateValue, end: DateValue): [DateValue, DateValue] {
  return start.compare(end) <= 0 ? [start, end] : [end, start]
}

function formatDateMillis(value?: number, exclusiveEnd = false): string {
  if (value === undefined) return ''

  const date = new Date(exclusiveEnd ? value - 1 : value)
  if (!Number.isFinite(date.getTime())) return ''

  return new Intl.DateTimeFormat(locale.value, {
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
  }).format(date)
}

function getDateFilterLabel(from?: number, to?: number): string {
  if (from === undefined && to === undefined) {
    return t('gallery.toolbar.filters.date')
  }

  const fromLabel = formatDateMillis(from)
  const toLabel = formatDateMillis(to, true)
  if (fromLabel && toLabel && fromLabel === toLabel) {
    return fromLabel
  }
  if (fromLabel && toLabel) {
    return t('gallery.toolbar.dateFilter.rangeLabel', { from: fromLabel, to: toLabel })
  }
  if (fromLabel) {
    return t('gallery.toolbar.dateFilter.fromLabel', { from: fromLabel })
  }
  return t('gallery.toolbar.dateFilter.toLabel', { to: toLabel })
}

function onDateRangeChange(value: DateRange) {
  draftDateRange.value = value
}

function applyDateFilter() {
  const start = draftDateRange.value.start
  if (!start) {
    clearDateFilter()
    return
  }

  const end = draftDateRange.value.end ?? start
  const [rangeStart, rangeEnd] = orderRangeDates(start, end)

  galleryView.setFilter({
    createdAtFrom: calendarDateToLocalStartMillis(rangeStart),
    createdAtTo: calendarDateToExclusiveEndMillis(rangeEnd),
  })
  datePopoverOpen.value = false
}

function clearDateFilter(event?: Event) {
  event?.preventDefault()
  event?.stopPropagation()
  galleryView.setFilter({
    createdAtFrom: undefined,
    createdAtTo: undefined,
  })
  draftDateRange.value = { start: undefined, end: undefined }
  datePopoverOpen.value = false
}

function keepDatePopoverForCalendarSelect(event: CustomEvent<{ originalEvent?: Event }>) {
  const originalEvent = event.detail?.originalEvent
  const target = originalEvent?.target ?? event.target
  if (!(target instanceof Element)) return

  // 年月 Select 的焦点切换会被外层 Popover 识别为 outside，需要保留日期面板。
  if (target.closest('[data-range-calendar-jump="true"]')) {
    event.preventDefault()
  }
}

function updateSearchQuery(query: string | number) {
  galleryView.setSearchQuery(String(query))
}

function clearSearch() {
  galleryView.setSearchQuery('')
}

function clearSearchFromTrigger(event: Event) {
  event.preventDefault()
  event.stopPropagation()
  clearSearch()
}

function onTypeFilterChange(value: string | number | bigint | Record<string, any> | null) {
  const stringValue = String(value || 'all')
  const type = stringValue === 'all' ? undefined : (stringValue as AssetType)
  galleryView.setTypeFilter(type)
}

function clearTypeFilter(event?: Event) {
  event?.preventDefault()
  event?.stopPropagation()
  galleryView.setTypeFilter(undefined)
}

function onReviewFlagChange(value: string | number | bigint | Record<string, any> | null) {
  const stringValue = String(value || 'all')
  galleryView.setFilter({
    reviewFlag: stringValue === 'all' ? undefined : (stringValue as ReviewFlag),
  })
}

function clearReviewFlagFilter(event?: Event) {
  event?.preventDefault()
  event?.stopPropagation()
  galleryView.setFilter({ reviewFlag: undefined })
}

function isRatingSelected(value: number): boolean {
  return selectedRatings.value.includes(value)
}

function setRatingFilters(values: number[]) {
  const ratings = normalizeRatings(values)
  galleryView.setFilter({ ratings: ratings.length > 0 ? ratings : undefined })
}

function toggleRatingFilter(value: number) {
  const current = selectedRatings.value
  if (current.includes(value)) {
    setRatingFilters(current.filter((rating) => rating !== value))
    return
  }

  setRatingFilters([...current, value])
}

function clearRatingFilter(event?: Event) {
  event?.preventDefault()
  event?.stopPropagation()
  galleryView.setFilter({ ratings: undefined })
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

function onCurrentFolderOnlyChange(value: boolean) {
  galleryView.setIncludeSubfolders(!value)
}

function applyColorFilter() {
  galleryView.setFilter({
    colorHex: draftColorHex.value,
    colorDistance: draftColorDistance.value,
  })
}

function clearColorFilter(event?: Event) {
  event?.preventDefault()
  event?.stopPropagation()
  galleryView.setFilter({
    colorHex: undefined,
    colorDistance: undefined,
  })
  draftColorHex.value = '#FFFFFF'
  draftColorDistance.value = COLOR_DISTANCE_DEFAULT
  colorPopoverOpen.value = false
}

function onColorDistanceChange(value: number[] | undefined) {
  if (value && value.length > 0 && value[0] !== undefined) {
    draftColorDistance.value = value[0]
  }
}

function clearAttributeFilters() {
  galleryView.clearAttributeFilters()
  draftColorHex.value = '#FFFFFF'
  draftColorDistance.value = COLOR_DISTANCE_DEFAULT
  draftDateRange.value = { start: undefined, end: undefined }
}

function isEditableTarget(target: EventTarget | null): boolean {
  if (!(target instanceof HTMLElement)) {
    return false
  }

  return target.isContentEditable || ['INPUT', 'TEXTAREA', 'SELECT'].includes(target.tagName)
}

function handleToolbarContextMenu(event: MouseEvent) {
  if (isEditableTarget(event.target)) {
    return
  }

  event.preventDefault()
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
    galleryView.setViewSizeFromSlider(value[0])
  }
}
</script>

<template>
  <div ref="toolbarRef" class="flex flex-col" @contextmenu="handleToolbarContextMenu">
    <div class="flex min-h-10 items-center justify-between gap-3 px-2 pt-1.5">
      <div class="flex min-w-0 items-center gap-2">
        <div
          class="flex min-w-0 items-center gap-2 rounded-md px-2 py-1 text-sm font-medium text-foreground"
          :title="currentSource.label"
        >
          <Images v-if="currentSource.type === 'all'" class="h-4 w-4 shrink-0 text-primary" />
          <Folder
            v-else-if="currentSource.type === 'folder'"
            class="h-4 w-4 shrink-0 text-primary"
          />
          <Tag v-else class="h-4 w-4 shrink-0 text-primary" />
          <span class="shrink-0">
            {{
              currentSource.type === 'folder'
                ? t('gallery.toolbar.browse.folder')
                : currentSource.type === 'tag'
                  ? t('gallery.toolbar.browse.tag')
                  : t('gallery.toolbar.browse.source')
            }}
          </span>
          <span class="min-w-0 truncate">{{ currentSource.label }}</span>
        </div>
      </div>

      <TooltipProvider :delay-duration="300">
        <div class="flex shrink-0 items-center gap-2">
          <div
            v-if="isWide"
            class="mr-2 flex w-28 items-center"
            :title="t('gallery.toolbar.thumbnailSize.label')"
          >
            <Slider
              :model-value="[currentSliderPosition]"
              @update:model-value="onViewSizeSliderChange"
              :min="0"
              :max="100"
              :step="1"
              class="w-full"
            />
          </div>

          <Tooltip v-if="currentSource.type === 'folder'">
            <TooltipTrigger as-child>
              <div class="inline-flex">
                <Toggle
                  size="sm"
                  :model-value="currentFolderOnly"
                  :aria-label="t('gallery.toolbar.folderOptions.currentFolderOnly')"
                  class="text-sidebar-foreground transition-colors duration-200 ease-out hover:bg-sidebar-hover hover:text-sidebar-accent-foreground focus-visible:border-transparent focus-visible:ring-2 focus-visible:ring-sidebar-ring focus-visible:ring-offset-2 data-[state=on]:bg-sidebar-accent data-[state=on]:text-sidebar-foreground data-[state=on]:hover:bg-sidebar-accent data-[state=on]:hover:text-sidebar-foreground"
                  @update:model-value="onCurrentFolderOnlyChange"
                >
                  <Folder class="h-4 w-4" />
                </Toggle>
              </div>
            </TooltipTrigger>
            <TooltipContent side="bottom">
              {{ t('gallery.toolbar.folderOptions.currentFolderOnly') }}
            </TooltipContent>
          </Tooltip>

          <Tooltip>
            <TooltipTrigger as-child>
              <span class="inline-flex">
                <DropdownMenu>
                  <DropdownMenuTrigger as-child>
                    <Button variant="sidebarGhost" size="icon-sm">
                      <ArrowUpDown class="h-4 w-4" />
                    </Button>
                  </DropdownMenuTrigger>
                  <DropdownMenuContent align="end" class="w-48">
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
                        <span class="pl-8">{{ t('gallery.toolbar.sort.resolution') }}</span>
                      </DropdownMenuRadioItem>
                      <DropdownMenuRadioItem value="size">
                        <span class="pl-8">{{ t('gallery.toolbar.sort.size') }}</span>
                      </DropdownMenuRadioItem>
                    </DropdownMenuRadioGroup>
                    <DropdownMenuSeparator />
                    <DropdownMenuItem @click="toggleSortOrder">
                      <ArrowUpDown class="mr-2 h-4 w-4" />
                      {{ sortOrderLabel }}
                    </DropdownMenuItem>
                  </DropdownMenuContent>
                </DropdownMenu>
              </span>
            </TooltipTrigger>
            <TooltipContent side="bottom">
              {{ t('gallery.toolbar.sort.label') }}
            </TooltipContent>
          </Tooltip>

          <Tooltip>
            <TooltipTrigger as-child>
              <span class="inline-flex">
                <Popover>
                  <PopoverTrigger as-child>
                    <Button variant="sidebarGhost" size="icon-sm">
                      <component :is="currentViewModeIcon" class="h-4 w-4" />
                    </Button>
                  </PopoverTrigger>
                  <PopoverContent align="end" class="w-72">
                    <div class="space-y-6">
                      <div class="space-y-3">
                        <p class="text-sm font-medium">{{ t('gallery.toolbar.viewMode.label') }}</p>
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

                      <div v-if="!isWide" class="border-t" />

                      <div v-if="!isWide" class="space-y-3">
                        <p class="text-sm font-medium">
                          {{ t('gallery.toolbar.thumbnailSize.label') }}
                        </p>
                        <Slider
                          :model-value="[currentSliderPosition]"
                          @update:model-value="onViewSizeSliderChange"
                          :min="0"
                          :max="100"
                          :step="1"
                          class="w-full"
                        />
                        <div class="flex justify-between text-xs">
                          <span>{{ t('gallery.toolbar.thumbnailSize.fine') }}</span>
                          <span>{{ t('gallery.toolbar.thumbnailSize.showcase') }}</span>
                        </div>
                      </div>
                    </div>
                  </PopoverContent>
                </Popover>
              </span>
            </TooltipTrigger>
            <TooltipContent side="bottom">
              {{ t('gallery.toolbar.viewSettings.tooltip') }}
            </TooltipContent>
          </Tooltip>

          <Tooltip v-if="showMapEntry">
            <TooltipTrigger as-child>
              <Button variant="sidebarGhost" size="icon-sm" @click="handleOpenMap">
                <Map class="h-4 w-4" />
              </Button>
            </TooltipTrigger>
            <TooltipContent side="bottom">
              {{ t('app.navigation.map') }}
            </TooltipContent>
          </Tooltip>

          <Tooltip>
            <TooltipTrigger as-child @mouseenter="isPreferencesTooltipAllowed = true">
              <Button variant="sidebarGhost" size="icon-sm" @click="handleOpenPreferences">
                <Settings class="h-4 w-4" />
              </Button>
            </TooltipTrigger>
            <TooltipContent v-if="isPreferencesTooltipAllowed" side="bottom">
              {{ t('gallery.preferences.open') }}
            </TooltipContent>
          </Tooltip>
        </div>
      </TooltipProvider>
    </div>

    <div class="flex min-h-10 items-center justify-between gap-1.5 px-2 pb-1.5">
      <ScrollArea ref="filterScrollAreaRef" class="min-w-0 flex-1" @wheel="handleFilterWheel">
        <TooltipProvider :delay-duration="300">
          <div class="flex items-center gap-1.5 py-0.5 pr-2">
            <Tooltip>
              <TooltipTrigger as-child>
                <span class="inline-flex">
                  <Popover>
                    <PopoverTrigger as-child>
                      <Button
                        :variant="searchQuery ? 'toolbarFilterActive' : 'toolbarFilter'"
                        :size="isFilterCompact ? 'icon-sm' : 'filter-sm'"
                        :class="{ 'has-[>svg]:!pl-2': !isFilterCompact }"
                      >
                        <Search class="h-4 w-4" />
                        <template v-if="!isFilterCompact">
                          <span class="min-w-0 truncate">
                            {{ searchQuery || t('gallery.toolbar.filters.keyword') }}
                          </span>
                          <span
                            v-if="searchQuery"
                            class="-mr-1 rounded p-0.5 hover:text-foreground"
                            @pointerdown.stop.prevent
                            @click="clearSearchFromTrigger"
                          >
                            <X class="h-3.5 w-3.5" />
                          </span>
                          <ChevronDown v-else class="h-3.5 w-3.5" />
                        </template>
                      </Button>
                    </PopoverTrigger>
                    <PopoverContent align="start" class="w-72 p-3">
                      <div class="space-y-2">
                        <p class="text-xs font-medium">
                          {{ t('gallery.toolbar.filters.keyword') }}
                        </p>
                        <div class="relative">
                          <Search class="absolute top-1/2 left-2.5 h-4 w-4 -translate-y-1/2" />
                          <Input
                            :model-value="searchQuery"
                            @update:model-value="updateSearchQuery"
                            :placeholder="t('gallery.toolbar.search.placeholder')"
                            class="h-8 pr-8 pl-9"
                          />
                          <Button
                            v-if="searchQuery"
                            type="button"
                            variant="sidebarGhost"
                            size="icon-xs"
                            class="absolute top-1/2 right-1.5 -translate-y-1/2"
                            @click="clearSearch"
                          >
                            <X class="h-3.5 w-3.5" />
                          </Button>
                        </div>
                      </div>
                    </PopoverContent>
                  </Popover>
                </span>
              </TooltipTrigger>
              <TooltipContent v-if="isFilterCompact" side="bottom">
                {{
                  searchQuery
                    ? `${t('gallery.toolbar.filters.keyword')}: ${searchQuery}`
                    : t('gallery.toolbar.filters.keyword')
                }}
              </TooltipContent>
            </Tooltip>

            <Tooltip>
              <TooltipTrigger as-child>
                <span class="inline-flex">
                  <Popover v-model:open="datePopoverOpen">
                    <PopoverTrigger as-child>
                      <Button
                        :variant="hasDisplayDateRange ? 'toolbarFilterActive' : 'toolbarFilter'"
                        :size="isFilterCompact ? 'icon-sm' : 'filter-sm'"
                      >
                        <CalendarClock class="h-4 w-4" />
                        <template v-if="!isFilterCompact">
                          <span class="min-w-0 truncate">{{ displayDateFilterLabel }}</span>
                          <span
                            v-if="hasDisplayDateRange"
                            class="-mr-1 rounded p-0.5 hover:text-foreground"
                            @pointerdown.stop.prevent
                            @click="clearDateFilter"
                          >
                            <X class="h-3.5 w-3.5" />
                          </span>
                          <ChevronDown v-else class="h-3.5 w-3.5" />
                        </template>
                      </Button>
                    </PopoverTrigger>
                    <PopoverContent
                      align="start"
                      class="w-auto p-3"
                      @focus-outside="keepDatePopoverForCalendarSelect"
                      @interact-outside="keepDatePopoverForCalendarSelect"
                    >
                      <div class="space-y-3">
                        <div class="flex items-center justify-between gap-3">
                          <div class="min-w-0">
                            <p class="text-xs font-medium">
                              {{ t('gallery.toolbar.dateFilter.title') }}
                            </p>
                            <p class="truncate text-[11px] text-muted-foreground">
                              {{ displayDateFilterLabel }}
                            </p>
                          </div>
                        </div>

                        <RangeCalendar
                          :model-value="draftDateRange"
                          @update:model-value="onDateRangeChange"
                          :locale="locale"
                          initial-focus
                          class="p-0"
                        />

                        <div class="flex justify-end gap-2">
                          <Button
                            variant="outline"
                            size="sm"
                            class="h-7 px-3 text-xs"
                            @click="clearDateFilter"
                          >
                            {{ t('gallery.toolbar.dateFilter.clear') }}
                          </Button>
                          <Button size="sm" class="h-7 px-3 text-xs" @click="applyDateFilter">
                            {{ t('gallery.toolbar.dateFilter.apply') }}
                          </Button>
                        </div>
                      </div>
                    </PopoverContent>
                  </Popover>
                </span>
              </TooltipTrigger>
              <TooltipContent v-if="isFilterCompact" side="bottom">
                {{
                  hasDisplayDateRange
                    ? `${t('gallery.toolbar.filters.date')}: ${displayDateFilterLabel}`
                    : t('gallery.toolbar.filters.date')
                }}
              </TooltipContent>
            </Tooltip>

            <Tooltip>
              <TooltipTrigger as-child>
                <span class="inline-flex">
                  <Popover v-model:open="colorPopoverOpen">
                    <PopoverTrigger as-child>
                      <Button
                        :variant="activeColorHex ? 'toolbarFilterActive' : 'toolbarFilter'"
                        :size="isFilterCompact ? 'icon-sm' : 'filter-sm'"
                      >
                        <Palette class="h-4 w-4" />
                        <span
                          v-if="activeColorHex"
                          class="h-3.5 w-3.5 shrink-0 rounded-full border border-foreground/50"
                          :style="{ backgroundColor: activeColorHex }"
                        />
                        <template v-if="!isFilterCompact">
                          <span class="min-w-0 truncate">
                            {{ activeColorHex || t('gallery.toolbar.filters.color') }}
                          </span>
                          <span
                            v-if="activeColorHex"
                            class="-mr-1 rounded p-0.5 hover:text-foreground"
                            @pointerdown.stop.prevent
                            @click="clearColorFilter"
                          >
                            <X class="h-3.5 w-3.5" />
                          </span>
                          <ChevronDown v-else class="h-3.5 w-3.5" />
                        </template>
                      </Button>
                    </PopoverTrigger>
                    <PopoverContent align="start" class="w-auto p-3">
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
                              <p class="truncate font-mono text-[11px]">
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

                        <div class="space-y-1">
                          <p class="text-xs font-medium">
                            {{ t('gallery.toolbar.colorFilter.distance.label') }}
                          </p>
                          <div class="flex justify-end">
                            <span class="font-mono text-[11px]">
                              {{ draftColorDistance }}
                            </span>
                          </div>
                          <Slider
                            :model-value="[draftColorDistance]"
                            @update:model-value="onColorDistanceChange"
                            :min="COLOR_DISTANCE_MIN"
                            :max="COLOR_DISTANCE_MAX"
                            :step="1"
                            class="w-full"
                          />
                        </div>

                        <div class="flex justify-end">
                          <Button size="sm" class="h-7 px-3 text-xs" @click="applyColorFilter">
                            {{ t('gallery.toolbar.colorFilter.apply') }}
                          </Button>
                        </div>
                      </div>
                    </PopoverContent>
                  </Popover>
                </span>
              </TooltipTrigger>
              <TooltipContent v-if="isFilterCompact" side="bottom">
                {{
                  activeColorHex
                    ? `${t('gallery.toolbar.filters.color')}: ${activeColorHex}`
                    : t('gallery.toolbar.filters.color')
                }}
              </TooltipContent>
            </Tooltip>

            <Tooltip>
              <TooltipTrigger as-child>
                <span class="inline-flex">
                  <DropdownMenu>
                    <DropdownMenuTrigger as-child>
                      <Button
                        :variant="filter.type ? 'toolbarFilterActive' : 'toolbarFilter'"
                        :size="isFilterCompact ? 'icon-sm' : 'filter-sm'"
                      >
                        <Video v-if="filter.type === 'video'" class="h-4 w-4" />
                        <Image v-else class="h-4 w-4" />
                        <template v-if="!isFilterCompact">
                          <span class="min-w-0 truncate">{{ typeFilterLabel }}</span>
                          <span
                            v-if="filter.type"
                            class="-mr-1 rounded p-0.5 hover:text-foreground"
                            @pointerdown.stop.prevent
                            @click="clearTypeFilter"
                          >
                            <X class="h-3.5 w-3.5" />
                          </span>
                          <ChevronDown v-else class="h-3.5 w-3.5" />
                        </template>
                      </Button>
                    </DropdownMenuTrigger>
                    <DropdownMenuContent align="start" class="w-48">
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
                      </DropdownMenuRadioGroup>
                    </DropdownMenuContent>
                  </DropdownMenu>
                </span>
              </TooltipTrigger>
              <TooltipContent v-if="isFilterCompact" side="bottom">
                {{
                  filter.type
                    ? `${t('gallery.toolbar.filters.fileType')}: ${typeFilterLabel}`
                    : t('gallery.toolbar.filters.fileType')
                }}
              </TooltipContent>
            </Tooltip>

            <Tooltip>
              <TooltipTrigger as-child>
                <span class="inline-flex">
                  <Popover>
                    <PopoverTrigger as-child>
                      <Button
                        :variant="
                          selectedRatings.length > 0 ? 'toolbarFilterActive' : 'toolbarFilter'
                        "
                        :size="isFilterCompact ? 'icon-sm' : 'filter-sm'"
                      >
                        <Star class="h-4 w-4" />
                        <template v-if="!isFilterCompact">
                          <span class="min-w-0 truncate">{{ ratingFilterLabel }}</span>
                          <span
                            v-if="selectedRatings.length > 0"
                            class="-mr-1 rounded p-0.5 hover:text-foreground"
                            @pointerdown.stop.prevent
                            @click="clearRatingFilter"
                          >
                            <X class="h-3.5 w-3.5" />
                          </span>
                          <ChevronDown v-else class="h-3.5 w-3.5" />
                        </template>
                      </Button>
                    </PopoverTrigger>
                    <PopoverContent align="start" class="w-48 p-2">
                      <div class="space-y-1">
                        <button
                          v-for="rating in [5, 4, 3, 2, 1, 0]"
                          :key="rating"
                          type="button"
                          role="checkbox"
                          :aria-checked="isRatingSelected(rating)"
                          class="relative flex w-full cursor-default items-center gap-2 rounded-sm px-2 py-1.5 text-left text-sm outline-hidden transition-colors select-none hover:bg-accent hover:text-accent-foreground"
                          @click="toggleRatingFilter(rating)"
                        >
                          <Checkbox
                            as="span"
                            :model-value="isRatingSelected(rating)"
                            class="pointer-events-none"
                          />
                          <span class="flex min-w-0 items-center gap-0.5">
                            <Star
                              v-for="s in STARS"
                              :key="s"
                              class="h-3.5 w-3.5 transition-colors"
                              :class="
                                rating > 0 && s <= rating
                                  ? 'fill-amber-400 text-amber-400'
                                  : 'text-foreground/40'
                              "
                            />
                          </span>
                        </button>

                        <div v-if="selectedRatings.length > 0" class="border-t pt-1">
                          <button
                            type="button"
                            class="relative flex w-full cursor-default items-center rounded-sm px-2 py-1.5 text-left text-sm text-muted-foreground outline-hidden transition-colors select-none hover:bg-accent hover:text-accent-foreground"
                            @click="clearRatingFilter"
                          >
                            {{ t('gallery.toolbar.filter.rating.clear') }}
                          </button>
                        </div>
                      </div>
                    </PopoverContent>
                  </Popover>
                </span>
              </TooltipTrigger>
              <TooltipContent v-if="isFilterCompact" side="bottom">
                {{
                  selectedRatings.length > 0
                    ? `${t('gallery.toolbar.filters.rating')}: ${ratingFilterLabel}`
                    : t('gallery.toolbar.filters.rating')
                }}
              </TooltipContent>
            </Tooltip>

            <Tooltip>
              <TooltipTrigger as-child>
                <span class="inline-flex">
                  <DropdownMenu>
                    <DropdownMenuTrigger as-child>
                      <Button
                        :variant="
                          filter.reviewFlag !== undefined ? 'toolbarFilterActive' : 'toolbarFilter'
                        "
                        :size="isFilterCompact ? 'icon-sm' : 'filter-sm'"
                      >
                        <Flag class="h-4 w-4" />
                        <template v-if="!isFilterCompact">
                          <span class="min-w-0 truncate">{{ reviewFlagFilterLabel }}</span>
                          <span
                            v-if="filter.reviewFlag !== undefined"
                            class="-mr-1 rounded p-0.5 hover:text-foreground"
                            @pointerdown.stop.prevent
                            @click="clearReviewFlagFilter"
                          >
                            <X class="h-3.5 w-3.5" />
                          </span>
                          <ChevronDown v-else class="h-3.5 w-3.5" />
                        </template>
                      </Button>
                    </DropdownMenuTrigger>
                    <DropdownMenuContent align="start" class="w-44">
                      <DropdownMenuRadioGroup
                        :model-value="filter.reviewFlag || 'all'"
                        @update:model-value="onReviewFlagChange"
                      >
                        <DropdownMenuRadioItem value="all">
                          {{ t('gallery.toolbar.filter.flag.all') }}
                        </DropdownMenuRadioItem>
                        <DropdownMenuRadioItem value="rejected">
                          <X class="mr-2 h-4 w-4" />
                          {{ t('gallery.toolbar.filter.flag.rejected') }}
                        </DropdownMenuRadioItem>
                        <DropdownMenuRadioItem value="none">
                          <Flag class="mr-2 h-4 w-4" />
                          {{ t('gallery.toolbar.filter.flag.none') }}
                        </DropdownMenuRadioItem>
                      </DropdownMenuRadioGroup>
                    </DropdownMenuContent>
                  </DropdownMenu>
                </span>
              </TooltipTrigger>
              <TooltipContent v-if="isFilterCompact" side="bottom">
                {{
                  filter.reviewFlag !== undefined
                    ? `${t('gallery.toolbar.filters.reviewFlag')}: ${reviewFlagFilterLabel}`
                    : t('gallery.toolbar.filters.reviewFlag')
                }}
              </TooltipContent>
            </Tooltip>
          </div>
        </TooltipProvider>
        <ScrollBar orientation="horizontal" class="h-1.5" />
      </ScrollArea>

      <div v-if="hasAttributeFilters" class="ml-2 shrink-0">
        <Button
          variant="default"
          size="sm"
          class="h-8 shrink-0 px-2.5 text-xs"
          @click="clearAttributeFilters"
        >
          <X class="h-4 w-4" />
          {{ t('gallery.toolbar.filters.clear') }}
        </Button>
      </div>
    </div>
  </div>
</template>
