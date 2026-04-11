<script setup lang="ts">
import { ref } from 'vue'
import { Star, X } from 'lucide-vue-next'
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from '@/components/ui/tooltip'
import { useI18n } from '@/composables/useI18n'
import type { ReviewFlag } from '../../types'

const props = defineProps<{
  rating: number
  reviewFlag: ReviewFlag
  ratingIndeterminate?: boolean
  flagIndeterminate?: boolean
}>()

const emit = defineEmits<{
  setRating: [rating: number]
  clearRating: []
  setFlag: [flag: ReviewFlag]
  clearFlag: []
}>()

const { t } = useI18n()

const hoverRating = ref(0)

function onStarClick(star: number) {
  if (!props.ratingIndeterminate && props.rating === star) {
    emit('clearRating')
  } else {
    emit('setRating', star)
  }
}

function onRejectedClick() {
  if (!props.flagIndeterminate && props.reviewFlag === 'rejected') {
    emit('clearFlag')
  } else {
    emit('setFlag', 'rejected')
  }
}

const STARS = [1, 2, 3, 4, 5] as const
</script>

<template>
  <TooltipProvider>
    <div class="flex items-center justify-between gap-3">
      <div class="flex items-center gap-0.5">
        <button
          v-for="star in STARS"
          :key="star"
          type="button"
          class="rounded p-0.5 transition-colors"
          :title="`${star} ${t('gallery.details.review.starLabel')}`"
          @mouseenter="hoverRating = star"
          @mouseleave="hoverRating = 0"
          @click="onStarClick(star)"
        >
          <Star
            class="h-4 w-4 transition-colors"
            :class="
              hoverRating > 0
                ? star <= hoverRating
                  ? 'fill-amber-400 text-amber-400'
                  : 'text-muted-foreground/30'
                : !ratingIndeterminate && star <= rating
                  ? 'fill-amber-400 text-amber-400'
                  : 'text-muted-foreground/30'
            "
          />
        </button>
      </div>

      <Tooltip>
        <TooltipTrigger as-child>
          <button
            type="button"
            class="rounded p-1 transition-colors"
            :class="
              !flagIndeterminate && reviewFlag === 'rejected'
                ? 'text-rose-500'
                : 'text-muted-foreground hover:text-foreground'
            "
            @click="onRejectedClick"
          >
            <X class="h-4 w-4" />
          </button>
        </TooltipTrigger>
        <TooltipContent>
          {{
            !flagIndeterminate && reviewFlag === 'rejected'
              ? t('gallery.details.review.clearFlag')
              : t('gallery.review.flag.rejected')
          }}
        </TooltipContent>
      </Tooltip>
    </div>
  </TooltipProvider>
</template>
