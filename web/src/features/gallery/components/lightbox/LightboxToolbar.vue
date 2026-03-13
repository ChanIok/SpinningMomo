<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useGalleryStore } from '../../store'

const ACTUAL_SIZE_EPSILON = 0.001

const emit = defineEmits<{
  back: []
  fit: []
  actual: []
  zoomIn: []
  zoomOut: []
  toggleFilmstrip: []
  toggleFullscreen: []
}>()

const { t } = useI18n()
const store = useGalleryStore()

const currentIndex = computed(() => store.lightbox.currentIndex)
const totalCount = computed(() => store.totalCount)
const selectedCount = computed(() => store.selection.selectedIds.size)
const showFilmstrip = computed(() => store.lightbox.showFilmstrip)
const isFullscreen = computed(() => store.lightbox.isFullscreen)
const isFitMode = computed(() => store.lightbox.fitMode === 'contain')
const isActualSize = computed(
  () =>
    store.lightbox.fitMode === 'actual' && Math.abs(store.lightbox.zoom - 1) <= ACTUAL_SIZE_EPSILON
)
const lightboxMode = computed(() => {
  if (isFitMode.value) {
    return t('gallery.lightbox.toolbar.fit')
  }

  return `${Math.round(store.lightbox.zoom * 100)}%`
})
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
            isFitMode
              ? 'bg-accent text-accent-foreground'
              : 'text-muted-foreground hover:bg-accent hover:text-accent-foreground'
          "
          @click="emit('fit')"
          :title="t('gallery.lightbox.toolbar.fitTitle')"
        >
          {{ t('gallery.lightbox.toolbar.fit') }}
        </button>

        <button
          class="inline-flex h-9 items-center justify-center rounded-md px-3 text-xs transition-colors"
          :class="
            isActualSize
              ? 'bg-accent text-accent-foreground'
              : 'text-muted-foreground hover:bg-accent hover:text-accent-foreground'
          "
          @click="emit('actual')"
          :title="t('gallery.lightbox.toolbar.actualTitle')"
        >
          100%
        </button>

        <button
          class="inline-flex h-9 w-9 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
          @click="emit('zoomOut')"
          :title="t('gallery.lightbox.toolbar.zoomOutTitle')"
        >
          <svg class="h-4 w-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M20 12H4" />
          </svg>
        </button>

        <button
          class="inline-flex h-9 w-9 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
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
        @click="emit('toggleFullscreen')"
        :title="
          isFullscreen
            ? t('gallery.lightbox.toolbar.exitFullscreenTitle')
            : t('gallery.lightbox.toolbar.fullscreenTitle')
        "
      >
        <svg
          v-if="isFullscreen"
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
