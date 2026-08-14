<script setup lang="ts">
import { useI18n } from '@/composables/useI18n'
import { useGalleryViewControls } from '../../composables'
import { useGalleryStore } from '../../store'

const { t } = useI18n()
const store = useGalleryStore()
const { currentSource } = useGalleryViewControls()
</script>

<template>
  <div v-if="store.isCompactWindow" class="px-4 pt-3 pb-4 select-none">
    <h1 class="truncate text-2xl font-bold tracking-tight text-foreground">
      {{ currentSource.label }}
    </h1>
    <p class="mt-0.5 text-xs text-muted-foreground">
      {{ t('gallery.details.itemCount', { count: store.totalCount }) }}
    </p>
  </div>
</template>
