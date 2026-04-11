<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import type { ReviewFlag } from '../../types'

const props = defineProps<{
  rating: number | undefined
  reviewFlag: ReviewFlag | undefined
}>()

const emit = defineEmits<{
  'update:rating': [value: number | undefined]
  'update:reviewFlag': [value: ReviewFlag | undefined]
}>()

const { t } = useI18n()

const STARS = [1, 2, 3, 4, 5] as const

const activeRating = computed(() => props.rating)
const activeFlag = computed(() => props.reviewFlag)

function onRatingClick(value: number | 'unrated') {
  const numeric = value === 'unrated' ? 0 : value
  emit('update:rating', activeRating.value === numeric ? undefined : numeric)
}
</script>

<template>
  <div class="space-y-3">
    <div class="space-y-1.5">
      <p class="text-xs font-medium">{{ t('gallery.toolbar.filter.rating.label') }}</p>
      <div class="flex flex-wrap gap-1">
        <button
          type="button"
          class="rounded px-2 py-1 text-xs transition-colors"
          :class="
            activeRating === undefined
              ? 'bg-accent text-accent-foreground'
              : 'bg-muted text-muted-foreground hover:text-foreground'
          "
          @click="emit('update:rating', undefined)"
        >
          {{ t('gallery.toolbar.filter.rating.all') }}
        </button>

        <button
          v-for="star in STARS"
          :key="star"
          type="button"
          class="flex items-center rounded px-2 py-1 transition-colors"
          :class="
            activeRating === star
              ? 'bg-accent text-accent-foreground'
              : 'bg-muted text-muted-foreground hover:text-foreground'
          "
          @click="onRatingClick(star)"
        >
          <svg
            v-for="s in STARS"
            :key="s"
            class="h-3 w-3 transition-colors"
            :class="s <= star ? 'text-amber-400' : 'text-muted-foreground/30'"
            viewBox="0 0 24 24"
            fill="currentColor"
          >
            <path
              d="M11.049 2.927c.3-.921 1.603-.921 1.902 0l1.519 4.674a1 1 0 00.95.69h4.915c.969 0 1.371 1.24.588 1.81l-3.976 2.888a1 1 0 00-.363 1.118l1.518 4.674c.3.922-.755 1.688-1.538 1.118l-3.976-2.888a1 1 0 00-1.176 0l-3.976 2.888c-.783.57-1.838-.197-1.538-1.118l1.518-4.674a1 1 0 00-.363-1.118l-3.976-2.888c-.784-.57-.38-1.81.588-1.81h4.914a1 1 0 00.951-.69l1.519-4.674z"
            />
          </svg>
        </button>

        <button
          type="button"
          class="rounded px-2 py-1 text-xs transition-colors"
          :class="
            activeRating === 0
              ? 'bg-accent text-accent-foreground'
              : 'bg-muted text-muted-foreground hover:text-foreground'
          "
          @click="onRatingClick('unrated')"
        >
          {{ t('gallery.toolbar.filter.rating.unrated') }}
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
