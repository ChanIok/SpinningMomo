import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { FolderTreeNode } from '@/types/folder'
import { screenshotAPI } from '@/api/screenshot'

export const useFolderStore = defineStore('folder', () => {
  const folderTree = ref<FolderTreeNode[]>([])
  const loading = ref(false)

  async function loadFolderTree() {
    if (loading.value) return
    
    try {
      loading.value = true
      folderTree.value = await screenshotAPI.getFolderTree()
    } catch (error) {
      console.error('Failed to load folder tree:', error)
      throw error
    } finally {
      loading.value = false
    }
  }

  return {
    folderTree,
    loading,
    loadFolderTree
  }
}) 