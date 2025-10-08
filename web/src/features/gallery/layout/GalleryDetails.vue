<script setup lang="ts">
import { computed } from 'vue'
import { Button } from '@/components/ui/button'
import { Separator } from '@/components/ui/separator'
import { useGalleryStore } from '../store'

const store = useGalleryStore()

const activeAsset = computed(() => {
  const activeId = store.selection.activeId
  if (!activeId) return null
  return store.assets.find((a) => a.id === activeId)
})

const selectedCount = computed(() => store.selectedCount)

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
  <div class="h-full overflow-auto border-l bg-background p-4">
    <!-- 空状态 - 未选择资产 -->
    <div v-if="!activeAsset && selectedCount === 0" class="flex h-full items-center justify-center">
      <div class="text-center text-muted-foreground">
        <svg
          xmlns="http://www.w3.org/2000/svg"
          width="48"
          height="48"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          stroke-width="2"
          stroke-linecap="round"
          stroke-linejoin="round"
          class="mx-auto mb-4 opacity-50"
        >
          <rect width="18" height="18" x="3" y="3" rx="2" ry="2" />
          <circle cx="9" cy="9" r="2" />
          <path d="m21 15-3.086-3.086a2 2 0 0 0-2.828 0L6 21" />
        </svg>
        <p class="text-sm">选择资产查看详情</p>
      </div>
    </div>

    <!-- 多选状态 -->
    <div v-else-if="selectedCount > 1" class="space-y-4">
      <div class="flex items-center justify-between">
        <h3 class="font-medium">批量操作</h3>
        <Button variant="ghost" size="icon" @click="store.clearSelection()">
          <svg
            xmlns="http://www.w3.org/2000/svg"
            width="16"
            height="16"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            stroke-width="2"
            stroke-linecap="round"
            stroke-linejoin="round"
          >
            <path d="M18 6 6 18" />
            <path d="m6 6 12 12" />
          </svg>
        </Button>
      </div>

      <div class="text-sm text-muted-foreground">已选中 {{ selectedCount }} 项</div>

      <Separator />

      <div class="space-y-2">
        <p class="text-sm font-medium">批量操作</p>
        <div class="text-xs text-muted-foreground">敬请期待...</div>
      </div>
    </div>

    <!-- 单个资产详情 -->
    <div v-else-if="activeAsset" class="space-y-4">
      <div class="flex items-center justify-between">
        <h3 class="font-medium">详情</h3>
        <Button variant="ghost" size="icon" @click="store.setActiveAsset(undefined)">
          <svg
            xmlns="http://www.w3.org/2000/svg"
            width="16"
            height="16"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            stroke-width="2"
            stroke-linecap="round"
            stroke-linejoin="round"
          >
            <path d="M18 6 6 18" />
            <path d="m6 6 12 12" />
          </svg>
        </Button>
      </div>

      <!-- 基本信息 -->
      <div>
        <h4 class="mb-2 text-sm font-medium">基本信息</h4>
        <div class="space-y-2 text-xs">
          <div class="flex justify-between gap-2">
            <span class="text-muted-foreground">文件名</span>
            <span class="max-w-32 truncate font-mono" :title="activeAsset.name">{{
              activeAsset.name
            }}</span>
          </div>
          <div class="flex justify-between gap-2">
            <span class="text-muted-foreground">类型</span>
            <span class="rounded bg-secondary px-2 py-0.5">{{ activeAsset.type }}</span>
          </div>
        </div>
      </div>

      <Separator />

      <!-- 尺寸信息 -->
      <div>
        <h4 class="mb-2 text-sm font-medium">尺寸信息</h4>
        <div class="space-y-2 text-xs">
          <div v-if="activeAsset.width && activeAsset.height" class="flex justify-between gap-2">
            <span class="text-muted-foreground">分辨率</span>
            <span>{{ activeAsset.width }} × {{ activeAsset.height }}</span>
          </div>
          <div v-if="activeAsset.size" class="flex justify-between gap-2">
            <span class="text-muted-foreground">文件大小</span>
            <span>{{ formatFileSize(activeAsset.size) }}</span>
          </div>
        </div>
      </div>

      <Separator />

      <!-- 文件路径 -->
      <div>
        <h4 class="mb-2 text-sm font-medium">存储路径</h4>
        <div class="text-xs">
          <p class="rounded bg-muted/50 p-2 font-mono break-all">{{ activeAsset.path }}</p>
        </div>
      </div>

      <Separator />

      <div class="py-4 text-center text-xs text-muted-foreground">更多功能敬请期待...</div>
    </div>
  </div>
</template>
