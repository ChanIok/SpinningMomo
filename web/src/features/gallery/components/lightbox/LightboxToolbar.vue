<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useGalleryStore } from '../../store'
import { Popover, PopoverContent, PopoverTrigger } from '@/components/ui/popover'
import ReviewFilterPopover from '../tags/ReviewFilterPopover.vue'

const ACTUAL_SIZE_EPSILON = 0.001

const emit = defineEmits<{
  back: []
  fit: []
  actual: []
  zoomIn: []
  zoomOut: []
  toggleFilmstrip: []
  toggleImmersive: []
}>()

const { t } = useI18n()
const store = useGalleryStore()

const currentIndex = computed(() => store.selection.activeIndex ?? 0)
const totalCount = computed(() => store.totalCount)
const selectedCount = computed(() => store.selection.selectedIds.size)
const showFilmstrip = computed(() => store.lightbox.showFilmstrip)
const isImmersive = computed(() => store.lightbox.isImmersive)
const currentAsset = computed(() => {
  const currentIndex = store.selection.activeIndex
  if (currentIndex === undefined) {
    return null
  }

  return store.getAssetsInRange(currentIndex, currentIndex)[0] ?? null
})
// 视频使用原生 controls，不适用灯箱图片的适屏/缩放语义。
const supportsZoom = computed(() => currentAsset.value?.type !== 'video')
const isFitMode = computed(() => store.lightbox.fitMode === 'contain')
const isActualSize = computed(
  () =>
    store.lightbox.fitMode === 'actual' && Math.abs(store.lightbox.zoom - 1) <= ACTUAL_SIZE_EPSILON
)
const lightboxMode = computed(() => {
  if (currentAsset.value?.type === 'video') {
    return t('gallery.toolbar.filter.type.video')
  }

  if (isFitMode.value) {
    return t('gallery.lightbox.toolbar.fit')
  }

  return `${Math.round(store.lightbox.zoom * 100)}%`
})

const hasReviewFilter = computed(
  () => store.filter.rating !== undefined || store.filter.reviewFlag !== undefined
)
</script>

<template>
  <div class="surface-top flex items-center justify-between border-b border-border px-4 py-3">
    <div class="flex min-w-0 items-center gap-3 text-foreground">
      <button
        class="inline-flex h-9 w-9 shrink-0 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
        @click="emit('back')"
        :title="t('gallery.lightbox.toolbar.backTitle')"
      >
        <svg class="h-5 w-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
          <path
            stroke-linecap="round"
            stroke-linejoin="round"
            stroke-width="2"
            d="M15 19l-7-7 7-7"
          />
        </svg>
      </button>

      <div class="flex min-w-0 items-center gap-3">
        <span class="shrink-0 text-sm font-medium">{{ currentIndex + 1 }} / {{ totalCount }}</span>
        <span class="truncate text-xs text-muted-foreground">{{ lightboxMode }}</span>
        <span v-if="selectedCount > 0" class="shrink-0 text-xs text-primary">
          {{ t('gallery.lightbox.toolbar.selected') }} {{ selectedCount }}
        </span>
      </div>
    </div>

    <div class="flex items-center gap-2">
      <div class="mr-2 flex items-center gap-1">
        <button
          class="inline-flex h-9 items-center justify-center rounded-md px-3 text-xs transition-colors"
          :class="
            !supportsZoom
              ? 'cursor-not-allowed text-muted-foreground/40'
              : isFitMode
                ? 'bg-accent text-accent-foreground'
                : 'text-muted-foreground hover:bg-accent hover:text-accent-foreground'
          "
          :disabled="!supportsZoom"
          @click="emit('fit')"
          :title="t('gallery.lightbox.toolbar.fitTitle')"
        >
          {{ t('gallery.lightbox.toolbar.fit') }}
        </button>

        <button
          class="inline-flex h-9 items-center justify-center rounded-md px-3 text-xs transition-colors"
          :class="
            !supportsZoom
              ? 'cursor-not-allowed text-muted-foreground/40'
              : isActualSize
                ? 'bg-accent text-accent-foreground'
                : 'text-muted-foreground hover:bg-accent hover:text-accent-foreground'
          "
          :disabled="!supportsZoom"
          @click="emit('actual')"
          :title="t('gallery.lightbox.toolbar.actualTitle')"
        >
          100%
        </button>

        <button
          class="inline-flex h-9 w-9 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
          :class="{
            'cursor-not-allowed text-muted-foreground/40 hover:bg-transparent': !supportsZoom,
          }"
          :disabled="!supportsZoom"
          @click="emit('zoomOut')"
          :title="t('gallery.lightbox.toolbar.zoomOutTitle')"
        >
          <svg class="h-4 w-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M20 12H4" />
          </svg>
        </button>

        <button
          class="inline-flex h-9 w-9 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
          :class="{
            'cursor-not-allowed text-muted-foreground/40 hover:bg-transparent': !supportsZoom,
          }"
          :disabled="!supportsZoom"
          @click="emit('zoomIn')"
          :title="t('gallery.lightbox.toolbar.zoomInTitle')"
        >
          <svg class="h-4 w-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path
              stroke-linecap="round"
              stroke-linejoin="round"
              stroke-width="2"
              d="M12 4v16m8-8H4"
            />
          </svg>
        </button>
      </div>

      <!-- 评分与标记筛选 -->
      <Popover>
        <PopoverTrigger as-child>
          <button
            class="inline-flex items-center justify-center rounded-md p-2 transition-colors hover:bg-accent hover:text-accent-foreground"
            :class="hasReviewFilter ? 'text-primary' : 'text-muted-foreground'"
            :title="t('gallery.toolbar.filter.review.tooltip')"
          >
            <svg class="h-5 w-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path
                stroke-linecap="round"
                stroke-linejoin="round"
                stroke-width="2"
                d="M11.049 2.927c.3-.921 1.603-.921 1.902 0l1.519 4.674a1 1 0 00.95.69h4.915c.969 0 1.371 1.24.588 1.81l-3.976 2.888a1 1 0 00-.363 1.118l1.518 4.674c.3.922-.755 1.688-1.538 1.118l-3.976-2.888a1 1 0 00-1.176 0l-3.976 2.888c-.783.57-1.838-.197-1.538-1.118l1.518-4.674a1 1 0 00-.363-1.118l-3.976-2.888c-.784-.57-.38-1.81.588-1.81h4.914a1 1 0 00.951-.69l1.519-4.674z"
              />
            </svg>
          </button>
        </PopoverTrigger>
        <PopoverContent align="end" class="w-56 p-3">
          <ReviewFilterPopover
            :rating="store.filter.rating"
            :review-flag="store.filter.reviewFlag"
            @update:rating="(v) => store.setFilter({ rating: v })"
            @update:review-flag="(v) => store.setFilter({ reviewFlag: v })"
          />
        </PopoverContent>
      </Popover>

      <button
        class="inline-flex items-center justify-center rounded-md p-2 text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
        @click="emit('toggleFilmstrip')"
        :title="
          showFilmstrip
            ? t('gallery.lightbox.toolbar.filmstripHideTitle')
            : t('gallery.lightbox.toolbar.filmstripShowTitle')
        "
      >
        <svg
          v-if="showFilmstrip"
          class="h-5 w-5"
          fill="none"
          stroke="currentColor"
          viewBox="0 0 24 24"
        >
          <path
            stroke-linecap="round"
            stroke-linejoin="round"
            stroke-width="2"
            d="M19 9l-7 7-7-7"
          />
        </svg>
        <svg v-else class="h-5 w-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 15l7-7 7 7" />
        </svg>
      </button>

      <button
        class="inline-flex items-center justify-center rounded-md p-2 text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
        @click="emit('toggleImmersive')"
        :title="
          isImmersive
            ? t('gallery.lightbox.toolbar.exitImmersiveTitle')
            : t('gallery.lightbox.toolbar.immersiveTitle')
        "
      >
        <svg
          v-if="isImmersive"
          class="h-5 w-5"
          fill="none"
          stroke="currentColor"
          viewBox="0 0 24 24"
        >
          <path
            stroke-linecap="round"
            stroke-linejoin="round"
            stroke-width="2"
            d="M8 3H5a2 2 0 00-2 2v3m18 0V5a2 2 0 00-2-2h-3m0 18h3a2 2 0 002-2v-3M3 16v3a2 2 0 002 2h3m-1-7l4-4m0 0h-3m3 0v3m-8 1l4-4m0 0v3m0-3H8"
          />
        </svg>
        <svg v-else class="h-5 w-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
          <path
            stroke-linecap="round"
            stroke-linejoin="round"
            stroke-width="2"
            d="M4 8V4m0 0h4M4 4l5 5m11-1V4m0 0h-4m4 0l-5 5M4 16v4m0 0h4m-4 0l5-5m11 5l-5-5m5 5v-4m0 4h-4"
          />
        </svg>
      </button>
    </div>
  </div>
</template>
