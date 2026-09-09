<script setup lang="ts">
import { Images } from '@lucide/vue'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()
</script>

<template>
  <div class="flex h-full w-full items-center justify-center p-4 select-none">
    <div class="text-center text-muted-foreground">
      <Images class="mx-auto mb-4 size-12 stroke-[1.5] opacity-50" />
      <p class="text-sm">{{ t('gallery.empty.allMedia') }}</p>
    </div>
  </div>
</template>
