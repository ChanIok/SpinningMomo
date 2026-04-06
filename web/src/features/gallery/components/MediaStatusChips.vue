<script setup lang="ts">
import { computed } from 'vue'
import { Star, X } from 'lucide-vue-next'
import type { ReviewFlag } from '../types'

interface MediaStatusChipsProps {
  rating?: number
  reviewFlag?: ReviewFlag
  compact?: boolean
}

const props = withDefaults(defineProps<MediaStatusChipsProps>(), {
  rating: 0,
  reviewFlag: 'none',
  compact: false,
})

const hasRating = computed(() => (props.rating ?? 0) > 0)
const isRejected = computed(() => props.reviewFlag === 'rejected')
</script>

<template>
  <div class="pointer-events-none absolute inset-0">
    <div
      v-if="hasRating"
      class="absolute flex items-center gap-1 rounded-md border text-white transition-opacity duration-150"
      :class="
        compact
          ? 'top-1 left-1 border-white/12 bg-black/45 px-1.5 py-0.5 text-[10px]'
          : 'top-2 left-2 border-white/15 bg-black/45 px-2 py-1 text-[11px]'
      "
    >
      <Star
        class="shrink-0 fill-current text-current"
        :class="compact ? 'h-2.5 w-2.5' : 'h-3 w-3'"
      />
      <span class="font-medium">{{ rating }}</span>
    </div>

    <!-- filmstrip：右下小方角标，仅 X -->
    <div
      v-if="isRejected && compact"
      class="absolute right-1 bottom-1 flex h-4 w-4 items-center justify-center rounded-sm border border-white/20 bg-black/50 text-current shadow-sm backdrop-blur-sm"
    >
      <X class="h-3 w-3 stroke-[3] text-rose-400" />
    </div>

    <!-- 主卡片：胶囊 + X + 文案 -->
    <div
      v-if="isRejected && !compact"
      class="absolute right-2 bottom-2 flex items-center gap-1 rounded-md border border-white/15 bg-black/50 px-2 py-1 text-[11px] text-white transition-opacity duration-150"
    >
      <X class="h-3.5 w-3.5 shrink-0 text-rose-400" />
      <span class="font-medium">弃置</span>
    </div>
  </div>
</template>
