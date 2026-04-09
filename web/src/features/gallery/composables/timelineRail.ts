import { computed, type ComputedRef, type Ref } from 'vue'
import type { TimelineBucket } from '../types'

export interface TimelineRailMarker {
  id: string
  contentOffset: number
  label?: string
}

export interface TimelineRailLabel {
  id: string
  text: string
  contentOffset: number
}

function formatMonthFull(monthStr: string, locale: string): string {
  const [yearStr, monthStrNum] = monthStr.split('-')
  const year = Number(yearStr)
  const month = Number(monthStrNum)
  if (!Number.isInteger(year) || !Number.isInteger(month) || month < 1 || month > 12) {
    return monthStr
  }

  const date = new Date(Date.UTC(year, month - 1, 1))
  return new Intl.DateTimeFormat(locale, {
    year: 'numeric',
    month: 'long',
    timeZone: 'UTC',
  }).format(date)
}

export function useTimelineRail(options: {
  isTimelineMode: Ref<boolean> | ComputedRef<boolean>
  buckets: Ref<TimelineBucket[]> | ComputedRef<TimelineBucket[]>
  locale: Ref<string> | ComputedRef<string>
  getOffsetByAssetIndex: (assetIndex: number) => number | undefined
}) {
  const markers = computed((): TimelineRailMarker[] => {
    if (!options.isTimelineMode.value || options.buckets.value.length === 0) {
      return []
    }

    const result: TimelineRailMarker[] = []
    let globalAssetIndex = 0

    for (const bucket of options.buckets.value) {
      const contentOffset = options.getOffsetByAssetIndex(globalAssetIndex)
      if (contentOffset !== undefined) {
        result.push({
          id: bucket.month,
          contentOffset,
          label: formatMonthFull(bucket.month, options.locale.value),
        })
      }

      globalAssetIndex += bucket.count
    }

    return result
  })

  const labels = computed((): TimelineRailLabel[] => {
    const result: TimelineRailLabel[] = []
    let currentYear: string | null = null

    for (const marker of markers.value) {
      const year = marker.id.split('-')[0]
      if (year && year !== currentYear) {
        result.push({
          id: year,
          text: year,
          contentOffset: marker.contentOffset,
        })
        currentYear = year
      }
    }

    return result
  })

  return {
    markers,
    labels,
  }
}
