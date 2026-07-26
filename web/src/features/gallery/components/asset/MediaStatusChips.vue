<script setup lang="ts">
import { computed } from 'vue'
import { Paintbrush, Star, X } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import type { ReviewFlag, Tag } from '../../types'

interface MediaStatusChipsProps {
  rating?: number
  reviewFlag?: ReviewFlag
  compact?: boolean
  showRating?: boolean
  hasDyeCode?: boolean
  showTags?: boolean
  tags?: Tag[]
}

const MAX_RENDERED_CARD_TAGS = 12

const props = withDefaults(defineProps<MediaStatusChipsProps>(), {
  rating: 0,
  reviewFlag: 'none',
  compact: false,
  showRating: true,
  hasDyeCode: false,
  showTags: false,
  tags: () => [],
})

const { t } = useI18n()
const hasRating = computed(() => props.showRating && (props.rating ?? 0) > 0)
const isRejected = computed(() => props.reviewFlag === 'rejected')
const renderedTags = computed(() =>
  props.showTags && !props.compact ? props.tags.slice(0, MAX_RENDERED_CARD_TAGS) : []
)
const hasMoreTags = computed(
  () => props.showTags && !props.compact && props.tags.length > MAX_RENDERED_CARD_TAGS
)
const tagTooltip = computed(() => props.tags.map((tag) => tag.name).join(' · '))
const hasStatusMarkers = computed(() => hasRating.value || props.hasDyeCode)
const hasTopContent = computed(() => hasStatusMarkers.value || renderedTags.value.length > 0)
</script>

<template>
  <div class="pointer-events-none absolute inset-0">
    <div
      v-if="hasTopContent"
      class="absolute flex flex-col items-start gap-1 overflow-hidden"
      :class="compact ? 'top-1 left-1' : 'top-2 right-2 bottom-12 left-2'"
    >
      <div v-if="hasStatusMarkers" class="flex shrink-0 items-start gap-1">
        <div
          v-if="hasRating"
          class="flex items-center gap-1 rounded-md border border-white/15 bg-black/45 text-white"
          :class="compact ? 'px-1.5 py-0.5 text-[10px]' : 'px-2 py-1 text-[11px]'"
        >
          <Star
            class="shrink-0 fill-current text-current"
            :class="compact ? 'h-2.5 w-2.5' : 'h-3 w-3'"
          />
          <span class="font-medium">{{ rating }}</span>
        </div>
        <div
          v-if="hasDyeCode"
          :title="t('gallery.preferences.badges.dyeCodeTooltip')"
          class="flex items-center justify-center rounded-md border border-white/15 bg-black/45 text-white"
          :class="compact ? 'h-4 w-4' : 'h-6 w-6'"
        >
          <Paintbrush :class="compact ? 'h-2.5 w-2.5' : 'h-3.5 w-3.5'" />
        </div>
      </div>

      <div
        v-if="renderedTags.length > 0"
        :title="tagTooltip"
        class="flex max-h-9 min-h-0 w-full flex-wrap content-start gap-1 overflow-hidden"
      >
        <div
          v-for="tag in renderedTags"
          :key="tag.id"
          class="max-w-full min-w-0 rounded border border-white/15 bg-black/45 px-1.5 py-0.5 text-[10px] leading-3 text-white shadow-sm backdrop-blur-[1px]"
        >
          <span class="block truncate font-medium">{{ tag.name }}</span>
        </div>
        <div
          v-if="hasMoreTags"
          class="rounded border border-white/15 bg-black/45 px-1.5 py-0.5 text-[10px] leading-3 text-white shadow-sm backdrop-blur-[1px]"
        >
          …
        </div>
      </div>
    </div>

    <!-- filmstrip：右下小方角标，仅 X -->
    <div
      v-if="isRejected && compact"
      :title="t('gallery.review.flag.rejected')"
      class="absolute right-1 bottom-1 flex h-4 w-4 items-center justify-center rounded-sm border border-white/20 bg-black/50 text-current shadow-sm backdrop-blur-sm"
    >
      <X class="h-3 w-3 stroke-[3] text-rose-400" />
    </div>

    <!-- 主卡片：胶囊 + X + 文案 -->
    <div
      v-if="isRejected && !compact"
      :title="t('gallery.review.flag.rejected')"
      class="absolute right-2 bottom-2 flex items-center gap-1 rounded-md border border-white/15 bg-black/50 px-2 py-1 text-[11px] text-white transition-opacity duration-150"
    >
      <X class="h-3.5 w-3.5 shrink-0 text-rose-400" />
      <span class="font-medium">{{ t('gallery.review.flag.rejected') }}</span>
    </div>
  </div>
</template>
