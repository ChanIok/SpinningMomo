<script setup lang="ts">
import { computed } from 'vue'
import { Star } from 'lucide-vue-next'
import { Checkbox } from '@/components/ui/checkbox'
import { useI18n } from '@/composables/useI18n'
import type { ReviewFlag } from '../../types'

const props = defineProps<{
  ratings: number[] | undefined
  reviewFlag: ReviewFlag | undefined
}>()

const emit = defineEmits<{
  'update:ratings': [value: number[] | undefined]
  'update:reviewFlag': [value: ReviewFlag | undefined]
}>()

const { t } = useI18n()

const STARS = [1, 2, 3, 4, 5] as const
const RATING_OPTIONS = [5, 4, 3, 2, 1, 0] as const

const activeRatings = computed(() => normalizeRatings(props.ratings))
const activeFlag = computed(() => props.reviewFlag)

function normalizeRatings(ratings?: number[]): number[] {
  return [...new Set(ratings ?? [])]
    .filter((rating) => Number.isInteger(rating) && rating >= 0 && rating <= 5)
    .sort((a, b) => b - a)
}

function isRatingSelected(value: number): boolean {
  return activeRatings.value.includes(value)
}

function emitRatings(values: number[]) {
  const ratings = normalizeRatings(values)
  emit('update:ratings', ratings.length > 0 ? ratings : undefined)
}

function toggleRating(value: number) {
  const current = activeRatings.value
  if (current.includes(value)) {
    emitRatings(current.filter((rating) => rating !== value))
    return
  }

  emitRatings([...current, value])
}
</script>

<template>
  <div class="space-y-3">
    <div class="space-y-1.5">
      <div class="flex items-center justify-between gap-2">
        <p class="text-xs font-medium">{{ t('gallery.toolbar.filter.rating.label') }}</p>
        <button
          v-if="activeRatings.length > 0"
          type="button"
          class="text-xs text-muted-foreground transition-colors hover:text-foreground"
          @click="emit('update:ratings', undefined)"
        >
          {{ t('gallery.toolbar.filter.rating.clear') }}
        </button>
      </div>

      <div class="space-y-1">
        <button
          v-for="rating in RATING_OPTIONS"
          :key="rating"
          type="button"
          role="checkbox"
          :aria-checked="isRatingSelected(rating)"
          class="flex h-8 w-full items-center gap-2 rounded-md px-2 text-left text-xs transition-colors hover:bg-sidebar-hover"
          @click="toggleRating(rating)"
        >
          <Checkbox as="span" :model-value="isRatingSelected(rating)" class="pointer-events-none" />
          <span class="flex min-w-0 items-center gap-0.5">
            <Star
              v-for="s in STARS"
              :key="s"
              class="h-3.5 w-3.5 transition-colors"
              :class="
                rating > 0 && s <= rating
                  ? 'fill-primary text-primary'
                  : 'fill-muted text-muted-foreground/30'
              "
            />
          </span>
        </button>
      </div>
    </div>

    <div class="border-t" />

    <div class="space-y-1.5">
      <p class="text-xs font-medium">{{ t('gallery.toolbar.filter.flag.label') }}</p>
      <div class="flex flex-wrap gap-1">
        <button
          type="button"
          class="rounded-full px-3 py-1 text-xs font-medium transition-colors"
          :class="
            activeFlag === undefined
              ? 'bg-accent text-accent-foreground'
              : 'bg-muted text-muted-foreground hover:text-foreground'
          "
          @click="emit('update:reviewFlag', undefined)"
        >
          {{ t('gallery.toolbar.filter.flag.all') }}
        </button>
        <button
          type="button"
          class="rounded-full px-3 py-1 text-xs font-medium transition-colors"
          :class="
            activeFlag === 'rejected'
              ? 'bg-rose-500/20 text-rose-600 dark:text-rose-400'
              : 'bg-muted text-muted-foreground hover:text-foreground'
          "
          @click="emit('update:reviewFlag', 'rejected')"
        >
          {{ t('gallery.review.flag.rejected') }}
        </button>
      </div>
    </div>
  </div>
</template>
