<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { Image as ImageIcon } from '@lucide/vue'
import { Button } from '@/components/ui/button'
import { HoverCard, HoverCardContent, HoverCardTrigger } from '@/components/ui/hover-card'
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from '@/components/ui/tooltip'
import { useI18n } from '@/composables/useI18n'
import { galleryApi } from '../../api'
import type { MissingAssetItem } from '../../types'

const props = defineProps<{
  item: MissingAssetItem
}>()

const { t } = useI18n()
const hasError = ref(false)

const thumbnailUrl = computed(() => galleryApi.getThumbnailUrl(props.item.hash))

watch(thumbnailUrl, () => {
  hasError.value = false
})
</script>

<template>
  <TooltipProvider v-if="!item.hash" :delay-duration="150">
    <Tooltip>
      <TooltipTrigger as-child>
        <Button
          variant="sidebarGhost"
          size="icon-xs"
          disabled
          class="cursor-not-allowed opacity-40"
          :aria-label="t('gallery.preferences.maintenance.noThumbnail')"
        >
          <ImageIcon class="h-3.5 w-3.5" />
        </Button>
      </TooltipTrigger>
      <TooltipContent side="top">
        {{ t('gallery.preferences.maintenance.noThumbnail') }}
      </TooltipContent>
    </Tooltip>
  </TooltipProvider>

  <HoverCard v-else :open-delay="150" :close-delay="100">
    <HoverCardTrigger as-child>
      <Button
        variant="sidebarGhost"
        size="icon-xs"
        :aria-label="t('gallery.preferences.maintenance.previewThumbnail')"
      >
        <ImageIcon class="h-3.5 w-3.5" />
      </Button>
    </HoverCardTrigger>
    <HoverCardContent
      side="left"
      align="center"
      :side-offset="6"
      class="w-fit overflow-hidden rounded-md border border-border bg-popover p-0 shadow-lg"
    >
      <img
        v-if="!hasError"
        :src="thumbnailUrl"
        :alt="item.name"
        class="block h-auto max-h-64 w-auto max-w-64"
        @error="hasError = true"
      />
      <div
        v-else
        class="flex min-h-36 w-64 flex-col items-center justify-center gap-1.5 bg-muted/20 p-4 text-center text-xs text-muted-foreground"
      >
        <ImageIcon class="h-6 w-6 opacity-40" />
        <span>{{ t('gallery.preferences.maintenance.thumbnailLoadFailed') }}</span>
      </div>
    </HoverCardContent>
  </HoverCard>
</template>
