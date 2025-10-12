<script setup lang="ts">
import { computed, onMounted } from 'vue'
import { Button } from '@/components/ui/button'
import { Separator } from '@/components/ui/separator'
import { useGalleryStore } from '../store'
import { useGallerySidebar, useGalleryData } from '../composables'
import FolderTreeItem from '../components/FolderTreeItem.vue'

const store = useGalleryStore()
const galleryData = useGalleryData()

const {
  folders,
  tags,
  sidebar,
  selectedFolder,
  selectedTag,
  loading,
  error,
  selectFolder,
  selectTag,
  selectAllMedia,
  addNewTag,
} = useGallerySidebar()

onMounted(() => {
  galleryData.loadFolderTree()
})

const totalCount = computed(() => store.foldersAssetTotalCount)
</script>

<template>
  <div class="flex h-full flex-col border-r bg-background">
    <!-- 标题栏 -->
    <div class="flex h-12 items-center justify-between border-b px-4">
      <h2 class="text-sm font-medium">资源库</h2>
      <Button variant="ghost" size="icon" class="h-7 w-7">
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
          <path
            d="M12.22 2h-.44a2 2 0 0 0-2 2v.18a2 2 0 0 1-1 1.73l-.43.25a2 2 0 0 1-2 0l-.15-.08a2 2 0 0 0-2.73.73l-.22.38a2 2 0 0 0 .73 2.73l.15.1a2 2 0 0 1 1 1.72v.51a2 2 0 0 1-1 1.74l-.15.09a2 2 0 0 0-.73 2.73l.22.38a2 2 0 0 0 2.73.73l.15-.08a2 2 0 0 1 2 0l.43.25a2 2 0 0 1 1 1.73V20a2 2 0 0 0 2 2h.44a2 2 0 0 0 2-2v-.18a2 2 0 0 1 1-1.73l.43-.25a2 2 0 0 1 2 0l.15.08a2 2 0 0 0 2.73-.73l.22-.39a2 2 0 0 0-.73-2.73l-.15-.08a2 2 0 0 1-1-1.74v-.5a2 2 0 0 1 1-1.74l.15-.09a2 2 0 0 0 .73-2.73l-.22-.38a2 2 0 0 0-2.73-.73l-.15.08a2 2 0 0 1-2 0l-.43-.25a2 2 0 0 1-1-1.73V4a2 2 0 0 0-2-2z"
          />
          <circle cx="12" cy="12" r="3" />
        </svg>
      </Button>
    </div>

    <!-- 导航菜单 -->
    <div class="flex-1 overflow-auto p-4">
      <!-- 所有媒体 -->
      <Button
        :variant="sidebar.activeSection === 'all' ? 'secondary' : 'ghost'"
        :class="['h-9 w-full justify-start gap-3', sidebar.activeSection === 'all' && 'bg-accent']"
        @click="selectAllMedia"
      >
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
          class="flex-shrink-0"
        >
          <path
            d="m21.64 3.64-1.28-1.28a1.21 1.21 0 0 0-1.72 0L2.36 18.64a1.21 1.21 0 0 0 0 1.72l1.28 1.28a1.2 1.2 0 0 0 1.72 0L21.64 5.36a1.2 1.2 0 0 0 0-1.72Z"
          />
          <path d="m14 7 3 3" />
          <path d="M5 6v4" />
          <path d="M19 14v4" />
          <path d="M10 2v2" />
          <path d="M7 8H3" />
          <path d="M21 16h-4" />
          <path d="M11 3H9" />
        </svg>
        <span class="flex-1 text-left">所有媒体</span>
        <span class="rounded bg-secondary px-1.5 py-0.5 text-xs">{{ totalCount }}</span>
      </Button>

      <!-- 文件夹区域 -->
      <div class="mt-4 space-y-2">
        <h3 class="px-2 text-xs font-medium tracking-wider text-muted-foreground uppercase">
          文件夹
        </h3>
        <!-- 加载状态 -->
        <div v-if="loading" class="px-2 text-xs text-muted-foreground">加载中...</div>
        <!-- 错误状态 -->
        <div v-else-if="error" class="px-2 text-xs text-destructive">
          {{ error }}
        </div>
        <!-- 文件夹树 -->
        <div v-else class="space-y-1">
          <FolderTreeItem
            v-for="folder in folders"
            :key="folder.id"
            :folder="folder"
            :selected-folder="selectedFolder"
            :depth="0"
            @select="selectFolder"
          />
        </div>
      </div>

      <Separator class="my-4" />

      <!-- 标签区域 -->
      <div class="space-y-2">
        <div class="flex items-center justify-between px-2">
          <h3 class="text-xs font-medium tracking-wider text-muted-foreground uppercase">标签</h3>
          <Button variant="ghost" size="icon" class="h-6 w-6" @click="addNewTag">
            <svg
              xmlns="http://www.w3.org/2000/svg"
              width="12"
              height="12"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
              stroke-linecap="round"
              stroke-linejoin="round"
            >
              <path d="M5 12h14" />
              <path d="M12 5v14" />
            </svg>
          </Button>
        </div>
        <div v-for="tag in tags" :key="tag.id">
          <Button
            :variant="selectedTag === tag.id ? 'secondary' : 'ghost'"
            :class="['h-8 w-full justify-start gap-2 px-2', selectedTag === tag.id && 'bg-accent']"
            @click="selectTag(tag.id, tag.name)"
          >
            <svg
              xmlns="http://www.w3.org/2000/svg"
              width="14"
              height="14"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
              stroke-linecap="round"
              stroke-linejoin="round"
              class="flex-shrink-0"
            >
              <path
                d="M12 2H2v10l9.29 9.29c.94.94 2.48.94 3.42 0l6.58-6.58c.94-.94.94-2.48 0-3.42L12 2Z"
              />
              <path d="M7 7h.01" />
            </svg>
            <span class="flex-1 text-left text-sm">{{ tag.name }}</span>
            <span class="rounded border px-1.5 py-0.5 text-xs">{{ tag.count }}</span>
          </Button>
        </div>
      </div>
    </div>

    <!-- 底部信息 -->
    <div class="border-t p-4">
      <div class="text-center text-xs text-muted-foreground">
        {{ totalCount > 0 ? `共 ${totalCount} 项` : '暂无项目' }}
      </div>
    </div>
  </div>
</template>
