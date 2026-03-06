<script setup lang="ts">
import { Separator } from '@/components/ui/separator'
import { useI18n } from '@/composables/useI18n'
import type { Asset } from '../types'

interface AssetDetailsContentProps {
  asset: Asset
  thumbnailUrl: string
}

defineProps<AssetDetailsContentProps>()

const { t } = useI18n()

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
</script>

<template>
  <!-- 资产缩略图 -->
  <div>
    <h4 class="mb-2 text-sm font-medium">{{ t('gallery.details.asset.preview') }}</h4>
    <div class="flex justify-center">
      <img :src="thumbnailUrl" :alt="asset.name" class="max-w-full rounded shadow-md" />
    </div>
  </div>

  <Separator />

  <!-- 基本信息 -->
  <div>
    <h4 class="mb-2 text-sm font-medium">{{ t('gallery.details.asset.basicInfo') }}</h4>
    <div class="space-y-2 text-xs">
      <div class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{ t('gallery.details.asset.fileName') }}</span>
        <span class="max-w-32 truncate font-mono" :title="asset.name">
          {{ asset.name }}
        </span>
      </div>
      <div class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{ t('gallery.details.asset.type') }}</span>
        <span class="rounded bg-secondary px-2 py-0.5">{{ getAssetTypeLabel(asset.type) }}</span>
      </div>
    </div>
  </div>

  <Separator />

  <!-- 尺寸信息 -->
  <div>
    <h4 class="mb-2 text-sm font-medium">{{ t('gallery.details.asset.sizeInfo') }}</h4>
    <div class="space-y-2 text-xs">
      <div v-if="asset.width && asset.height" class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{ t('gallery.details.asset.resolution') }}</span>
        <span>{{ asset.width }} × {{ asset.height }}</span>
      </div>
      <div v-if="asset.size" class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{ t('gallery.details.asset.fileSize') }}</span>
        <span>{{ formatFileSize(asset.size) }}</span>
      </div>
    </div>
  </div>

  <slot name="after-size" />

  <Separator />

  <!-- 文件路径 -->
  <div>
    <h4 class="mb-2 text-sm font-medium">{{ t('gallery.details.asset.storagePath') }}</h4>
    <div class="text-xs">
      <p class="rounded bg-muted/50 p-2 font-mono break-all">{{ asset.path }}</p>
    </div>
  </div>
</template>
