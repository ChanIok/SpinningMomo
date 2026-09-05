<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import type { TimelineHeaderKind } from '../../composables/timelineLayout'

const props = withDefaults(
  defineProps<{
    kind: TimelineHeaderKind
    date?: string
    month: string
    count: number
    compact?: boolean
    startIndex?: number
    endIndex?: number
    isAllSelected?: boolean
    isMultiSelect?: boolean
  }>(),
  {
    compact: false,
    isAllSelected: false,
    isMultiSelect: false,
  }
)

const emit = defineEmits<{
  (e: 'selectGroup', payload: { startIndex?: number; endIndex?: number }): void
}>()

const isBadgeInteractive = computed(() => {
  if (props.compact) {
    return props.isMultiSelect
  }
  return true
})

function handleBadgeClick() {
  if (!isBadgeInteractive.value) {
    return
  }
  emit('selectGroup', { startIndex: props.startIndex, endIndex: props.endIndex })
}

const { locale, t } = useI18n()

function parseCalendarDate(value: string | undefined): Date | null {
  if (!value) {
    return null
  }

  const match = /^(\d{4})-(\d{2})-(\d{2})$/.exec(value)
  if (!match) {
    return null
  }

  const year = Number(match[1])
  const month = Number(match[2])
  const day = Number(match[3])
  if (month < 1 || month > 12 || day < 1 || day > 31) {
    return null
  }

  return new Date(year, month - 1, day, 12)
}

function parseCalendarMonth(value: string): Date | null {
  const match = /^(\d{4})-(\d{2})$/.exec(value)
  if (!match) {
    return null
  }

  const year = Number(match[1])
  const month = Number(match[2])
  if (month < 1 || month > 12) {
    return null
  }

  return new Date(year, month - 1, 1, 12)
}

function getCalendarDateKey(date: Date): string {
  return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(
    date.getDate()
  ).padStart(2, '0')}`
}

function getYesterday(date: Date): Date {
  return new Date(date.getFullYear(), date.getMonth(), date.getDate() - 1, 12)
}

const monthLabel = computed(() => {
  const date = parseCalendarMonth(props.month)
  if (!date) {
    return props.month
  }

  const options: Intl.DateTimeFormatOptions = {
    month: props.compact ? 'short' : 'long',
  }
  if (date.getFullYear() !== new Date().getFullYear()) {
    options.year = 'numeric'
  }

  return new Intl.DateTimeFormat(locale.value, options).format(date)
})

interface TimelineHeaderDisplay {
  primary: string
  secondary?: string
}

const headerDisplay = computed<TimelineHeaderDisplay>(() => {
  if (props.kind === 'month') {
    return { primary: monthLabel.value }
  }

  const date = parseCalendarDate(props.date)
  if (!date) {
    return { primary: props.date ?? '' }
  }

  const today = new Date()
  const dateKey = getCalendarDateKey(date)
  if (dateKey === getCalendarDateKey(today)) {
    return { primary: t('gallery.timeline.today') }
  }

  if (dateKey === getCalendarDateKey(getYesterday(today))) {
    return { primary: t('gallery.timeline.yesterday') }
  }

  const dateOptions: Intl.DateTimeFormatOptions = {
    month: props.compact ? 'short' : 'long',
    day: 'numeric',
  }
  if (date.getFullYear() !== today.getFullYear()) {
    dateOptions.year = 'numeric'
  }

  const primary = new Intl.DateTimeFormat(locale.value, dateOptions).format(date)
  const secondary = new Intl.DateTimeFormat(locale.value, { weekday: 'short' }).format(date)

  return { primary, secondary }
})

const badgeText = computed(() => {
  if (props.compact && props.isMultiSelect) {
    return props.isAllSelected ? t('gallery.timeline.deselectAll') : t('gallery.timeline.selectAll')
  }
  return t('gallery.details.itemCount', { count: props.count })
})

const badgeVariantClass = computed(() => {
  if (props.isAllSelected) {
    return 'bg-foreground text-background font-medium hover:bg-foreground/90'
  }

  // 紧凑模式多选状态下，赋予实体浅灰胶囊背景以提供明确的可点击感
  if (props.compact && props.isMultiSelect) {
    return 'bg-muted text-foreground font-medium hover:bg-muted/80'
  }

  // 桌面端保持轻量 Ghost 风格
  return 'bg-transparent text-muted-foreground hover:bg-muted hover:text-foreground'
})
</script>

<template>
  <div
    :data-timeline-header="props.kind"
    class="flex items-center justify-between gap-3"
    :class="props.compact ? 'h-9 px-4' : 'h-11 px-1'"
  >
    <h2
      class="flex min-w-0 items-center text-foreground select-none"
      :class="props.compact ? 'text-sm' : 'text-base'"
    >
      <span class="truncate">{{ headerDisplay.primary }}</span>
      <template v-if="headerDisplay.secondary">
        <span
          class="mx-2 inline-block shrink-0 bg-border"
          :class="props.compact ? 'h-3 w-px' : 'h-3.5 w-px'"
          aria-hidden="true"
        />
        <span class="shrink-0 text-xs text-foreground/80 sm:text-sm">
          {{ headerDisplay.secondary }}
        </span>
      </template>
    </h2>
    <button
      v-if="isBadgeInteractive"
      type="button"
      class="flex shrink-0 cursor-pointer items-center justify-center rounded-full px-2 py-1 text-xs transition-colors select-none"
      :class="badgeVariantClass"
      @click.stop="handleBadgeClick"
    >
      {{ badgeText }}
    </button>
    <span
      v-else
      class="flex shrink-0 cursor-default items-center justify-center rounded-full bg-transparent px-2 py-1 text-xs text-muted-foreground select-none"
    >
      {{ badgeText }}
    </span>
  </div>
</template>
