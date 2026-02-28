<script setup lang="ts">
import { Separator } from '@/components/ui/separator'
import type { Asset } from '../types'

interface AssetDetailsContentProps {
  asset: Asset
  thumbnailUrl: string
}

defineProps<AssetDetailsContentProps>()

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
</script>

<template>
  <!-- 资产缩略图 -->
  <div>
    <h4 class="mb-2 text-sm font-medium">预览</h4>
    <div class="flex justify-center">
      <img :src="thumbnailUrl" :alt="asset.name" class="max-w-full rounded shadow-md" />
    </div>
  </div>

  <Separator />

  <!-- 基本信息 -->
  <div>
    <h4 class="mb-2 text-sm font-medium">基本信息</h4>
    <div class="space-y-2 text-xs">
      <div class="flex justify-between gap-2">
        <span class="text-muted-foreground">文件名</span>
        <span class="max-w-32 truncate font-mono" :title="asset.name">
          {{ asset.name }}
        </span>
      </div>
      <div class="flex justify-between gap-2">
        <span class="text-muted-foreground">类型</span>
        <span class="rounded bg-secondary px-2 py-0.5">{{ asset.type }}</span>
      </div>
    </div>
  </div>

  <Separator />

  <!-- 尺寸信息 -->
  <div>
    <h4 class="mb-2 text-sm font-medium">尺寸信息</h4>
    <div class="space-y-2 text-xs">
      <div v-if="asset.width && asset.height" class="flex justify-between gap-2">
        <span class="text-muted-foreground">分辨率</span>
        <span>{{ asset.width }} × {{ asset.height }}</span>
      </div>
      <div v-if="asset.size" class="flex justify-between gap-2">
        <span class="text-muted-foreground">文件大小</span>
        <span>{{ formatFileSize(asset.size) }}</span>
      </div>
    </div>
  </div>

  <slot name="after-size" />

  <Separator />

  <!-- 文件路径 -->
  <div>
    <h4 class="mb-2 text-sm font-medium">存储路径</h4>
    <div class="text-xs">
      <p class="rounded bg-muted/50 p-2 font-mono break-all">{{ asset.path }}</p>
    </div>
  </div>
</template>
