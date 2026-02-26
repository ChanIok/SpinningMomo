<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { Button } from '@/components/ui/button'
import { Separator } from '@/components/ui/separator'
import { useGallerySidebar, useGalleryData } from '../composables'
import FolderTreeItem from '../components/FolderTreeItem.vue'
import TagTreeItem from '../components/TagTreeItem.vue'
import TagInlineEditor from '../components/TagInlineEditor.vue'

const galleryData = useGalleryData()

const {
  folders,
  foldersLoading,
  foldersError,
  tags,
  tagsLoading,
  tagsError,
  selectedFolder,
  selectedTag,
  selectFolder,
  selectTag,
  loadTagTree,
  createTag,
  updateTag,
  deleteTag,
} = useGallerySidebar()

// 标签创建状态
const isCreatingTag = ref(false)

function startCreateTag() {
  isCreatingTag.value = true
}

async function handleCreateTag(name: string) {
  try {
    await createTag(name)
    isCreatingTag.value = false
  } catch (error) {
    console.error('Failed to create tag:', error)
  }
}

function handleCancelCreateTag() {
  isCreatingTag.value = false
}

async function handleRenameTag(tagId: number, newName: string) {
  try {
    await updateTag(tagId, newName)
  } catch (error) {
    console.error('Failed to rename tag:', error)
  }
}

async function handleCreateChildTag(parentId: number, name: string) {
  try {
    await createTag(name, parentId)
  } catch (error) {
    console.error('Failed to create child tag:', error)
  }
}

async function handleDeleteTag(tagId: number) {
  try {
    await deleteTag(tagId)
  } catch (error) {
    console.error('Failed to delete tag:', error)
  }
}

onMounted(() => {
  galleryData.loadFolderTree()
  loadTagTree()
})
</script>

<template>
  <div class="flex h-full flex-col">
    <!-- 导航菜单 -->
    <div class="flex-1 overflow-auto p-4">
      <!-- 文件夹区域 -->
      <div class="mt-4 space-y-2">
        <h3 class="px-2 text-xs font-medium tracking-wider text-muted-foreground uppercase">
          文件夹
        </h3>
        <!-- 加载状态 -->
        <div v-if="foldersLoading" class="px-2 text-xs text-muted-foreground">加载中...</div>
        <!-- 错误状态 -->
        <div v-else-if="foldersError" class="px-2 text-xs text-destructive">
          {{ foldersError }}
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
          <Button variant="ghost" size="icon" class="h-6 w-6" @click="startCreateTag">
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
        <!-- 加载状态 -->
        <div v-if="tagsLoading" class="px-2 text-xs text-muted-foreground">加载中...</div>
        <!-- 错误状态 -->
        <div v-else-if="tagsError" class="px-2 text-xs text-destructive">
          {{ tagsError }}
        </div>
        <!-- 标签树 -->
        <div v-else class="space-y-1">
          <!-- 快速创建标签 -->
          <div v-if="isCreatingTag" class="px-2">
            <TagInlineEditor
              placeholder="输入标签名..."
              @confirm="handleCreateTag"
              @cancel="handleCancelCreateTag"
            />
          </div>
          <!-- 标签列表 -->
          <TagTreeItem
            v-for="tag in tags"
            :key="tag.id"
            :tag="tag"
            :selected-tag="selectedTag"
            :depth="0"
            @select="selectTag"
            @rename="handleRenameTag"
            @create-child="handleCreateChildTag"
            @delete="handleDeleteTag"
          />
        </div>
      </div>
    </div>
  </div>
</template>
