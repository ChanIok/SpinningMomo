<script setup lang="ts">
import { Separator } from '@/components/ui/separator'
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from '@/components/ui/tooltip'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { copyToClipboard } from '@/lib/utils'
import type { Asset } from '../types'

interface AssetDetailsContentProps {
  asset: Asset
  thumbnailUrl: string
}

const props = defineProps<AssetDetailsContentProps>()

const { t } = useI18n()
const { toast } = useToast()

function formatFileSize(bytes: number): string {
  const units = ['B', 'KB', 'MB', 'GB']
  let size = bytes
  let unitIndex = 0
  while (size >= 1024 && unitIndex < units.length - 1) {
    size /= 1024
    unitIndex++
  }
  return `${size.toFixed(unitIndex === 0 ? 0 : 1)} ${units[unitIndex]}`
}

function getAssetTypeLabel(type: Asset['type']): string {
  switch (type) {
    case 'photo':
      return t('gallery.toolbar.filter.type.photo')
    case 'video':
      return t('gallery.toolbar.filter.type.video')
    case 'live_photo':
      return t('gallery.toolbar.filter.type.livePhoto')
    default:
      return t('gallery.details.assetType.unknown')
  }
}

async function handleCopyFileName() {
  const success = await copyToClipboard(props.asset.name)
  if (success) {
    toast.success(t('gallery.details.asset.copyFileNameSuccess'))
  } else {
    toast.error(t('gallery.details.asset.copyFileNameFailed'))
  }
}
</script>

<template>
  <div class="space-y-3">
    <div class="flex justify-center">
      <img
        :src="thumbnailUrl"
        :alt="asset.name"
        class="max-h-[180px] max-w-full rounded object-contain shadow-md"
      />
    </div>
    <slot name="after-preview" />
  </div>

  <Separator />

  <slot name="before-info" />

  <div>
    <h4 class="mb-2 text-sm font-medium">{{ t('gallery.details.asset.basicInfo') }}</h4>
    <div class="space-y-2 text-xs">
      <div class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{ t('gallery.details.asset.fileName') }}</span>
        <TooltipProvider>
          <Tooltip>
            <TooltipTrigger as-child>
              <button
                type="button"
                class="max-w-32 cursor-pointer truncate text-right font-mono transition-colors hover:text-foreground/80 focus:outline-none focus-visible:ring-1 focus-visible:ring-ring"
                @click="handleCopyFileName"
              >
                {{ asset.name }}
              </button>
            </TooltipTrigger>
            <TooltipContent>
              <p class="max-w-80 font-mono text-xs break-all">{{ asset.name }}</p>
            </TooltipContent>
          </Tooltip>
        </TooltipProvider>
      </div>
      <div class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{ t('gallery.details.asset.type') }}</span>
        <span class="rounded bg-secondary px-2 py-0.5">{{ getAssetTypeLabel(asset.type) }}</span>
      </div>
      <div v-if="asset.width && asset.height" class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{ t('gallery.details.asset.resolution') }}</span>
        <span>{{ asset.width }} × {{ asset.height }}</span>
      </div>
      <div v-if="asset.size" class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{ t('gallery.details.asset.fileSize') }}</span>
        <span>{{ formatFileSize(asset.size) }}</span>
      </div>
      <div class="flex items-center justify-between gap-2">
        <span class="text-muted-foreground">{{ t('gallery.details.asset.description') }}</span>
        <div class="min-w-0 flex-1">
          <slot name="description">
            <p class="rounded bg-muted/50 px-2 py-1.5 text-xs break-words">
              {{ asset.description }}
            </p>
          </slot>
        </div>
      </div>
    </div>
  </div>

  <slot name="after-info" />
</template>
