<script setup lang="ts">
import { computed } from 'vue'
import { Button } from '@/components/ui/button'
import { Separator } from '@/components/ui/separator'
import { useGalleryStore } from '../store'
import { useGalleryData } from '../composables/useGalleryData'
import type { FolderTreeNode, Asset } from '../types'

const store = useGalleryStore()

// 获取详情面板焦点
const detailsFocus = computed(() => store.detailsPanel)

// 根据焦点获取对应数据
const currentFolder = computed(() => {
  if (detailsFocus.value.type !== 'folder') return null
  const folderId = detailsFocus.value.folderId
  return findFolderById(store.folders, folderId)
})

const activeAsset = computed(() => {
  if (detailsFocus.value.type !== 'asset') return null
  const assetId = detailsFocus.value.assetId

  // 在普通模式下查找
  if (!store.isTimelineMode) {
    return store.assets.find((a: Asset) => a.id === assetId)
  }

  // 在时间线模式下查找
  // 由于timelineMonthData不存在，我们暂时从store.assets中查找
  // TODO: 实现时间线模式下的资产查找逻辑
  return store.assets.find((a: Asset) => a.id === assetId)
})

// 使用gallery数据composable
const { getAssetThumbnailUrl } = useGalleryData()

const selectedCount = computed(() => store.selectedCount)

// 计算缩略图URL
const thumbnailUrl = computed(() => {
  if (!activeAsset.value) return ''
  return getAssetThumbnailUrl(activeAsset.value)
})

/**
 * 递归查找文件夹节点
 */
function findFolderById(folders: FolderTreeNode[], id: number): FolderTreeNode | null {
  for (const folder of folders) {
    if (folder.id === id) return folder
    if (folder.children) {
      const found = findFolderById(folder.children, id)
      if (found) return found
    }
  }
  return null
}

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
    <!-- 文件夹详情 -->
    <div v-if="detailsFocus.type === 'folder' && currentFolder" class="space-y-4">
      <div class="flex items-center justify-between">
        <h3 class="font-medium">详情</h3>
      </div>

      <!-- 文件夹信息 -->
      <div>
        <h4 class="mb-2 text-sm font-medium">文件夹信息</h4>
        <div class="space-y-2 text-xs">
          <div class="flex justify-between gap-2">
            <span class="text-muted-foreground">显示名称</span>
            <span
              class="truncate font-medium"
              :title="currentFolder.displayName || currentFolder.name"
            >
              {{ currentFolder.displayName || currentFolder.name }}
            </span>
          </div>
          <div class="flex justify-between gap-2">
            <span class="text-muted-foreground">文件夹名</span>
            <span class="truncate font-mono" :title="currentFolder.name">{{
              currentFolder.name
            }}</span>
          </div>
          <div class="flex flex-col gap-1">
            <span class="text-muted-foreground">完整路径</span>
            <p class="rounded bg-muted/50 p-2 font-mono text-xs break-all">
              {{ currentFolder.path }}
            </p>
          </div>
          <div class="flex justify-between gap-2">
            <span class="text-muted-foreground">资产数量</span>
            <span>{{ currentFolder.assetCount }} 项</span>
          </div>
        </div>
      </div>

      <Separator />

      <div class="py-4 text-center text-xs text-muted-foreground">更多功能敬请期待...</div>
    </div>

    <!-- 资产详情 -->
    <div v-else-if="detailsFocus.type === 'asset' && activeAsset" class="space-y-4">
      <div class="flex items-center justify-between">
        <h3 class="font-medium">详情</h3>
        <Button variant="ghost" size="icon" @click="store.clearDetailsFocus()">
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

      <!-- 资产缩略图 -->
      <div>
        <h4 class="mb-2 text-sm font-medium">预览</h4>
        <div class="flex justify-center">
          <img :src="thumbnailUrl" :alt="activeAsset.name" class="max-w-full rounded shadow-md" />
        </div>
      </div>

      <Separator />

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

    <!-- 批量操作 -->
    <div v-else-if="detailsFocus.type === 'batch'" class="space-y-4">
      <div class="flex items-center justify-between">
        <h3 class="font-medium">批量操作</h3>
        <Button variant="ghost" size="icon" @click="store.clearDetailsFocus()">
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

    <!-- 空状态 -->
    <div v-else class="flex h-full items-center justify-center">
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
        <p class="text-sm">选择资产或文件夹查看详情</p>
      </div>
    </div>
  </div>
</template>
