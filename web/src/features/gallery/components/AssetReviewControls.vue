<script setup lang="ts">
import { ref } from 'vue'
import { Star, Heart, X } from 'lucide-vue-next'
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from '@/components/ui/tooltip'
import { useI18n } from '@/composables/useI18n'
import type { ReviewFlag } from '../types'

const props = defineProps<{
  rating: number
  reviewFlag: ReviewFlag
  ratingIndeterminate?: boolean
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

function onFlagClick(flag: ReviewFlag) {
  if (!props.ratingIndeterminate && props.reviewFlag === flag) {
    emit('clearFlag')
  } else {
    emit('setFlag', flag)
  }
}

const STARS = [1, 2, 3, 4, 5] as const
</script>

<template>
  <TooltipProvider>
    <div class="flex items-center justify-between">
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

      <div class="flex items-center">
        <Tooltip>
          <TooltipTrigger as-child>
            <button
              type="button"
              class="rounded p-1 transition-colors"
              :class="
                !ratingIndeterminate && reviewFlag === 'picked'
                  ? 'text-emerald-500'
                  : 'text-muted-foreground hover:text-foreground'
              "
              @click="onFlagClick('picked')"
            >
              <Heart
                class="h-4 w-4"
                :class="!ratingIndeterminate && reviewFlag === 'picked' ? 'fill-emerald-500' : ''"
              />
            </button>
          </TooltipTrigger>
          <TooltipContent>
            {{ t('gallery.review.flag.picked') }}
          </TooltipContent>
        </Tooltip>

        <Tooltip>
          <TooltipTrigger as-child>
            <button
              type="button"
              class="rounded p-1 transition-colors"
              :class="
                !ratingIndeterminate && reviewFlag === 'rejected'
                  ? 'text-rose-500'
                  : 'text-muted-foreground hover:text-foreground'
              "
              @click="onFlagClick('rejected')"
            >
              <X class="h-4 w-4" />
            </button>
          </TooltipTrigger>
          <TooltipContent>
            {{ t('gallery.review.flag.rejected') }}
          </TooltipContent>
        </Tooltip>
      </div>
    </div>
  </TooltipProvider>
</template>
