import { computed, ref, shallowRef, watch } from 'vue'
import { CalendarDate } from '@internationalized/date'
import type { DateRange, DateValue } from 'reka-ui'
import { useI18n } from '@/composables/useI18n'
import { useGalleryStore } from '../store'
import type { AssetFilter, AssetShape, AssetType, ReviewFlag } from '../types'

export const COLOR_DISTANCE_MIN = 1
export const COLOR_DISTANCE_MAX = 40
export const COLOR_DISTANCE_DEFAULT = 18
export const GALLERY_FILTER_STARS = [1, 2, 3, 4, 5] as const

function normalizeRatings(ratings?: number[]): number[] {
  return [...new Set(ratings ?? [])]
    .filter((rating) => Number.isInteger(rating) && rating >= 0 && rating <= 5)
    .sort((a, b) => b - a)
}

export function useGalleryFilterControls() {
  const { t, locale } = useI18n()
  const store = useGalleryStore()

  const filter = computed(() => store.filter)
  const searchQuery = computed(() => filter.value.searchQuery || '')
  const activeColorHex = computed(() => filter.value.colorHex)
  const activeColorDistance = computed(
    () =>
      (filter.value as AssetFilter & { colorDistance?: number }).colorDistance ??
      COLOR_DISTANCE_DEFAULT
  )
  const activeDateFrom = computed(() => filter.value.createdAtFrom)
  const activeDateTo = computed(() => filter.value.createdAtTo)
  const selectedRatings = computed(() => normalizeRatings(filter.value.ratings))

  const colorPopoverOpen = ref(false)
  const draftColorHex = ref(activeColorHex.value || '#FFFFFF')
  const draftColorDistance = ref(activeColorDistance.value)
  const datePopoverOpen = ref(false)
  const draftDateRange = shallowRef<DateRange>({ start: undefined, end: undefined })

  const activeFilterCount = computed(() => {
    return [
      Boolean(searchQuery.value),
      activeDateFrom.value !== undefined || activeDateTo.value !== undefined,
      filter.value.type !== undefined,
      filter.value.shape !== undefined,
      selectedRatings.value.length > 0,
      filter.value.reviewFlag !== undefined,
      Boolean(filter.value.colorHex),
    ].filter(Boolean).length
  })
  const hasAttributeFilters = computed(() => activeFilterCount.value > 0)

  const typeFilterLabel = computed(() => getTypeLabel(filter.value.type))
  const shapeFilterLabel = computed(() => getShapeLabel(filter.value.shape))
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
      displayDateRangeMillis.value.from !== undefined ||
      displayDateRangeMillis.value.to !== undefined
  )

  function syncFilterDrafts() {
    draftColorHex.value = activeColorHex.value || '#FFFFFF'
    draftColorDistance.value = activeColorDistance.value
    draftDateRange.value = {
      start: millisToCalendarDate(activeDateFrom.value),
      end: millisToCalendarDate(activeDateTo.value, true),
    }
  }

  watch(colorPopoverOpen, (open) => {
    if (open) {
      syncFilterDrafts()
    }
  })

  watch(datePopoverOpen, (open) => {
    if (open) {
      syncFilterDrafts()
    }
  })

  function getTypeLabel(type?: AssetType): string {
    if (type === 'photo') return t('gallery.toolbar.filter.type.photo')
    if (type === 'video') return t('gallery.toolbar.filter.type.video')
    return t('gallery.toolbar.filters.fileType')
  }

  function getShapeLabel(shape?: AssetShape): string {
    if (shape === 'landscape') return t('gallery.toolbar.filter.shape.landscape')
    if (shape === 'portrait') return t('gallery.toolbar.filter.shape.portrait')
    if (shape === 'square') return t('gallery.toolbar.filter.shape.square')
    return t('gallery.toolbar.filters.shape')
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
      return t('gallery.toolbar.filters.ratingRange', { min: minRating, max: maxRating })
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

    return new CalendarDate(
      date.getFullYear(),
      date.getMonth() + 1,
      date.getDate()
    ) as unknown as DateValue
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
    store.setFilter({
      createdAtFrom: calendarDateToLocalStartMillis(rangeStart),
      createdAtTo: calendarDateToExclusiveEndMillis(rangeEnd),
    })
    datePopoverOpen.value = false
  }

  function clearDateFilter(event?: Event) {
    event?.preventDefault()
    event?.stopPropagation()
    store.setFilter({ createdAtFrom: undefined, createdAtTo: undefined })
    draftDateRange.value = { start: undefined, end: undefined }
    datePopoverOpen.value = false
  }

  function keepDatePopoverForCalendarSelect(event: CustomEvent<{ originalEvent?: Event }>) {
    const originalEvent = event.detail?.originalEvent
    const target = originalEvent?.target ?? event.target
    if (!(target instanceof Element)) return

    if (target.closest('[data-range-calendar-jump="true"]')) {
      event.preventDefault()
    }
  }

  function updateSearchQuery(query: string | number) {
    store.setFilter({ searchQuery: String(query).trim() || undefined })
  }

  function clearSearch() {
    store.setFilter({ searchQuery: undefined })
  }

  function clearSearchFromTrigger(event: Event) {
    event.preventDefault()
    event.stopPropagation()
    clearSearch()
  }

  function onTypeFilterChange(value: string | number | bigint | Record<string, any> | null) {
    const stringValue = String(value || 'all')
    const type = stringValue === 'all' ? undefined : (stringValue as AssetType)
    store.setFilter({ type })
  }

  function clearTypeFilter(event?: Event) {
    event?.preventDefault()
    event?.stopPropagation()
    store.setFilter({ type: undefined })
  }

  function onShapeFilterChange(value: string | number | bigint | Record<string, any> | null) {
    const stringValue = String(value || 'all')
    const shape = stringValue === 'all' ? undefined : (stringValue as AssetShape)
    store.setFilter({ shape })
  }

  function clearShapeFilter(event?: Event) {
    event?.preventDefault()
    event?.stopPropagation()
    store.setFilter({ shape: undefined })
  }

  function onReviewFlagChange(value: string | number | bigint | Record<string, any> | null) {
    const stringValue = String(value || 'all')
    store.setFilter({ reviewFlag: stringValue === 'all' ? undefined : (stringValue as ReviewFlag) })
  }

  function clearReviewFlagFilter(event?: Event) {
    event?.preventDefault()
    event?.stopPropagation()
    store.setFilter({ reviewFlag: undefined })
  }

  function isRatingSelected(value: number): boolean {
    return selectedRatings.value.includes(value)
  }

  function setRatingFilters(values: number[]) {
    const ratings = normalizeRatings(values)
    store.setFilter({ ratings: ratings.length > 0 ? ratings : undefined })
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
    store.setFilter({ ratings: undefined })
  }

  function applyColorFilter() {
    store.setFilter({ colorHex: draftColorHex.value, colorDistance: draftColorDistance.value })
  }

  function clearColorFilter(event?: Event) {
    event?.preventDefault()
    event?.stopPropagation()
    store.setFilter({ colorHex: undefined, colorDistance: undefined })
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
    store.setFilter({
      searchQuery: undefined,
      createdAtFrom: undefined,
      createdAtTo: undefined,
      type: undefined,
      shape: undefined,
      ratings: undefined,
      reviewFlag: undefined,
      colorHex: undefined,
      colorDistance: undefined,
    })
    draftColorHex.value = '#FFFFFF'
    draftColorDistance.value = COLOR_DISTANCE_DEFAULT
    draftDateRange.value = { start: undefined, end: undefined }
  }

  return {
    filter,
    searchQuery,
    activeColorHex,
    activeColorDistance,
    activeDateFrom,
    activeDateTo,
    selectedRatings,
    typeFilterLabel,
    shapeFilterLabel,
    ratingFilterLabel,
    reviewFlagFilterLabel,
    activeFilterCount,
    hasAttributeFilters,
    colorPopoverOpen,
    draftColorHex,
    draftColorDistance,
    datePopoverOpen,
    draftDateRange,
    displayDateFilterLabel,
    hasDisplayDateRange,
    syncFilterDrafts,
    onDateRangeChange,
    applyDateFilter,
    clearDateFilter,
    keepDatePopoverForCalendarSelect,
    updateSearchQuery,
    clearSearch,
    clearSearchFromTrigger,
    onTypeFilterChange,
    clearTypeFilter,
    onShapeFilterChange,
    clearShapeFilter,
    onReviewFlagChange,
    clearReviewFlagFilter,
    isRatingSelected,
    toggleRatingFilter,
    clearRatingFilter,
    applyColorFilter,
    clearColorFilter,
    onColorDistanceChange,
    clearAttributeFilters,
    COLOR_DISTANCE_MIN,
    COLOR_DISTANCE_MAX,
    COLOR_DISTANCE_DEFAULT,
    STARS: GALLERY_FILTER_STARS,
  }
}
