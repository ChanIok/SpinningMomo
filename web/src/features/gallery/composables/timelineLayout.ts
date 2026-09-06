import type { DateGrouping, TimelineBucket } from '../types'

export type TimelineHeaderKind = 'month' | 'day'

export interface TimelineSection {
  date?: string
  month: string
  startIndex: number
  endIndex: number
  count: number
  monthStart: boolean
  monthCount: number
}

export interface TimelineDaySection extends TimelineSection {
  date: string
}

export interface TimelineHeaderDescriptor {
  kind: TimelineHeaderKind
  date?: string
  month: string
  count: number
  startIndex: number
  endIndex: number
}

function normalizeBucketCount(count: number): number {
  return Number.isFinite(count) ? Math.max(0, Math.trunc(count)) : 0
}

export function getTimelineBucketTotal(buckets: TimelineBucket[]): number {
  return buckets.reduce((total, bucket) => total + normalizeBucketCount(bucket.count), 0)
}

export function hasCompleteTimelineBuckets(buckets: TimelineBucket[], totalCount: number): boolean {
  return getTimelineBucketTotal(buckets) === Math.max(0, totalCount)
}

/**
 * 把后端的按日计数转换成连续的全局资产索引区间。
 * 标题只消费这些区间，不进入资产分页和选择索引。
 */
export function buildTimelineDaySections(
  buckets: TimelineBucket[],
  totalCount = getTimelineBucketTotal(buckets)
): TimelineDaySection[] {
  const boundedTotal = Math.max(0, totalCount)
  const sections: TimelineDaySection[] = []
  let assetIndex = 0
  let previousMonth: string | undefined

  for (const bucket of buckets) {
    const count = normalizeBucketCount(bucket.count)
    if (count === 0 || assetIndex >= boundedTotal) {
      continue
    }

    const startIndex = assetIndex
    const endIndex = Math.min(boundedTotal, startIndex + count)
    if (endIndex <= startIndex) {
      continue
    }

    sections.push({
      date: bucket.date,
      month: bucket.month,
      startIndex,
      endIndex,
      count: endIndex - startIndex,
      monthStart: bucket.month !== previousMonth,
      monthCount: 0,
    })

    assetIndex = endIndex
    previousMonth = bucket.month
  }

  const monthCounts = new Map<string, number>()
  for (const section of sections) {
    monthCounts.set(section.month, (monthCounts.get(section.month) ?? 0) + section.count)
  }

  return sections.map((section) => ({
    ...section,
    monthCount: monthCounts.get(section.month) ?? section.count,
  }))
}

export function buildTimelineMonthSections(
  buckets: TimelineBucket[],
  totalCount = getTimelineBucketTotal(buckets)
): TimelineSection[] {
  const daySections = buildTimelineDaySections(buckets, totalCount)
  const sections: TimelineSection[] = []

  for (const daySection of daySections) {
    const previousSection = sections[sections.length - 1]
    if (previousSection?.month === daySection.month) {
      previousSection.endIndex = daySection.endIndex
      previousSection.count += daySection.count
      continue
    }

    sections.push({
      month: daySection.month,
      startIndex: daySection.startIndex,
      endIndex: daySection.endIndex,
      count: daySection.count,
      monthStart: true,
      monthCount: daySection.monthCount,
    })
  }

  return sections
}

export function buildTimelineSections(
  buckets: TimelineBucket[],
  grouping: DateGrouping,
  totalCount = getTimelineBucketTotal(buckets)
): TimelineSection[] {
  if (grouping === 'month') {
    return buildTimelineMonthSections(buckets, totalCount)
  }

  if (grouping === 'day') {
    return buildTimelineDaySections(buckets, totalCount)
  }

  return []
}

export function getTimelineHeaderDescriptors(
  section: TimelineSection,
  grouping: DateGrouping
): TimelineHeaderDescriptor[] {
  const headers: TimelineHeaderDescriptor[] = []

  if (grouping === 'month' && section.monthStart) {
    headers.push({
      kind: 'month',
      month: section.month,
      count: section.monthCount,
      startIndex: section.startIndex,
      endIndex: section.endIndex,
    })
  }

  if (grouping === 'day' && section.date !== undefined) {
    headers.push({
      kind: 'day',
      date: section.date,
      month: section.month,
      count: section.count,
      startIndex: section.startIndex,
      endIndex: section.endIndex,
    })
  }

  return headers
}

export function getTimelineHeaderHeight(compact: boolean): number {
  return compact ? 36 : 44
}
