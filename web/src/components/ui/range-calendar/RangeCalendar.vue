<script lang="ts" setup>
import type { DateValue, RangeCalendarRootEmits, RangeCalendarRootProps } from 'reka-ui'
import type { HTMLAttributes } from 'vue'
import { computed, shallowRef, watch } from 'vue'
import { CalendarDate, getLocalTimeZone, today } from '@internationalized/date'
import { reactiveOmit } from '@vueuse/core'
import { RangeCalendarRoot, useForwardPropsEmits } from 'reka-ui'
import { cn } from '@/lib/utils'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import {
  RangeCalendarCell,
  RangeCalendarCellTrigger,
  RangeCalendarGrid,
  RangeCalendarGridBody,
  RangeCalendarGridHead,
  RangeCalendarGridRow,
  RangeCalendarHeadCell,
  RangeCalendarHeading,
  RangeCalendarNextButton,
  RangeCalendarPrevButton,
} from '.'

const YEAR_START = 2000
const YEAR_END_OFFSET = 2

const props = defineProps<RangeCalendarRootProps & { class?: HTMLAttributes['class'] }>()

const emits = defineEmits<RangeCalendarRootEmits>()

const delegatedProps = reactiveOmit(props, 'class', 'placeholder', 'numberOfMonths')

const forwarded = useForwardPropsEmits(delegatedProps, emits)

const controlledPlaceholder = shallowRef<DateValue | undefined>(props.placeholder)

const currentYear = new Date().getFullYear()
const yearOptions = Array.from(
  { length: currentYear + YEAR_END_OFFSET - YEAR_START + 1 },
  (_, index) => YEAR_START + index
)

const monthOptions = computed(() => {
  const formatter = new Intl.DateTimeFormat(props.locale, { month: 'short' })

  return Array.from({ length: 12 }, (_, index) => {
    const month = index + 1
    return {
      value: String(month),
      label: formatter.format(new Date(2024, index, 1)),
    }
  })
})

const visibleDate = computed(
  () =>
    controlledPlaceholder.value ??
    props.placeholder ??
    props.modelValue?.start ??
    props.defaultValue?.start ??
    today(getLocalTimeZone())
)
const selectedMonth = computed(() => String(visibleDate.value.month))
const selectedYear = computed(() => String(visibleDate.value.year))

watch(
  () => props.placeholder,
  (placeholder) => {
    if (placeholder) {
      controlledPlaceholder.value = placeholder
    }
  }
)

function handlePlaceholderChange(date: DateValue) {
  controlledPlaceholder.value = date
  emits('update:placeholder', date)
}

function changeVisibleMonth(value: string | number | bigint | Record<string, any> | null) {
  const month = Number(String(value ?? ''))
  if (!Number.isInteger(month)) {
    return
  }

  handlePlaceholderChange(new CalendarDate(visibleDate.value.year, month, visibleDate.value.day))
}

function changeVisibleYear(value: string | number | bigint | Record<string, any> | null) {
  const year = Number(String(value ?? ''))
  if (!Number.isInteger(year)) {
    return
  }

  handlePlaceholderChange(new CalendarDate(year, visibleDate.value.month, visibleDate.value.day))
}
</script>

<template>
  <RangeCalendarRoot
    v-slot="{ grid, weekDays }"
    data-slot="range-calendar"
    :class="cn('p-3', props.class)"
    v-bind="forwarded"
    :number-of-months="1"
    :placeholder="controlledPlaceholder"
    @update:placeholder="handlePlaceholderChange"
  >
    <RangeCalendarHeading class="sr-only" />

    <div>
      <div v-for="month in grid" :key="month.value.toString()" class="w-full">
        <div class="mb-3 flex h-8 items-center justify-between gap-2">
          <RangeCalendarPrevButton
            class="static size-7 bg-transparent p-0 opacity-60 hover:opacity-100"
          />

          <div class="flex min-w-0 items-center gap-1.5">
            <Select :model-value="selectedYear" @update:model-value="changeVisibleYear">
              <SelectTrigger
                data-range-calendar-jump="true"
                size="sm"
                class="h-7 min-w-18 px-2 text-xs"
              >
                <SelectValue />
              </SelectTrigger>
              <SelectContent data-range-calendar-jump="true" class="max-h-72">
                <SelectItem v-for="year in yearOptions" :key="year" :value="String(year)">
                  {{ year }}
                </SelectItem>
              </SelectContent>
            </Select>

            <Select :model-value="selectedMonth" @update:model-value="changeVisibleMonth">
              <SelectTrigger
                data-range-calendar-jump="true"
                size="sm"
                class="h-7 min-w-18 px-2 text-xs"
              >
                <SelectValue />
              </SelectTrigger>
              <SelectContent data-range-calendar-jump="true" class="max-h-72">
                <SelectItem
                  v-for="monthOption in monthOptions"
                  :key="monthOption.value"
                  :value="monthOption.value"
                >
                  {{ monthOption.label }}
                </SelectItem>
              </SelectContent>
            </Select>
          </div>

          <RangeCalendarNextButton
            class="static size-7 bg-transparent p-0 opacity-60 hover:opacity-100"
          />
        </div>

        <RangeCalendarGrid>
          <RangeCalendarGridHead>
            <RangeCalendarGridRow>
              <RangeCalendarHeadCell v-for="day in weekDays" :key="day">
                {{ day }}
              </RangeCalendarHeadCell>
            </RangeCalendarGridRow>
          </RangeCalendarGridHead>
          <RangeCalendarGridBody>
            <RangeCalendarGridRow
              v-for="(weekDates, index) in month.rows"
              :key="`weekDate-${index}`"
              class="mt-2 w-full"
            >
              <RangeCalendarCell
                v-for="weekDate in weekDates"
                :key="weekDate.toString()"
                :date="weekDate"
              >
                <RangeCalendarCellTrigger :day="weekDate" :month="month.value" />
              </RangeCalendarCell>
            </RangeCalendarGridRow>
          </RangeCalendarGridBody>
        </RangeCalendarGrid>
      </div>
    </div>
  </RangeCalendarRoot>
</template>
