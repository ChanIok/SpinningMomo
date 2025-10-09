import { ref, computed, onMounted } from 'vue'
import { useGalleryStore } from '../store'
import type { FolderTreeNode } from '../types'
import { getFolderTree } from '../api'

export interface Tag {
  id: string
  name: string
  count: number
}

/**
 * Gallery 侧边栏管理 Composable
 * 管理文件夹树和标签的逻辑
 */
export function useGallerySidebar() {
  const store = useGalleryStore()

  // ============= 本地状态 =============

  // 文件夹展开状态
  const expandedFolders = ref<Set<number>>(new Set())

  // 文件夹树数据（从后端获取）
  const folders = ref<FolderTreeNode[]>([])
  const loading = ref(false)
  const error = ref<string | null>(null)

  const tags = ref<Tag[]>([
    { id: 'tag-1', name: '收藏', count: 0 },
    { id: 'tag-2', name: '重要', count: 0 },
  ])

  // ============= 计算属性 =============

  const sidebar = computed(() => store.sidebar)
  const selectedFolder = computed(() => {
    if (sidebar.value.activeSection === 'folders' && store.filter.folderId) {
      const folderId = Number(store.filter.folderId)
      return isNaN(folderId) ? null : folderId
    }
    return null
  })
  const selectedTag = computed(() =>
    sidebar.value.activeSection === 'tags' ? store.filter.tagId : null
  )

  // ============= 操作方法 =============

  /**
   * 加载文件夹树
   */
  async function loadFolderTree() {
    loading.value = true
    error.value = null
    try {
      folders.value = await getFolderTree()
      console.log('📁 文件夹树加载成功')
    } catch (e) {
      error.value = '加载文件夹树失败'
      console.error('加载文件夹树失败:', e)
    } finally {
      loading.value = false
    }
  }

  /**
   * 切换文件夹展开/收起
   */
  function toggleFolderExpanded(folderId: number) {
    if (expandedFolders.value.has(folderId)) {
      expandedFolders.value.delete(folderId)
    } else {
      expandedFolders.value.add(folderId)
    }
  }

  /**
   * 检查文件夹是否展开
   */
  function isFolderExpanded(folderId: number): boolean {
    return expandedFolders.value.has(folderId)
  }

  /**
   * 选择文件夹
   */
  function selectFolder(folderId: number, folderName: string) {
    store.setSidebarActiveSection('folders')
    store.setFilter({ folderId: String(folderId) })
    console.log('📁 选择文件夹:', folderName)
  }

  /**
   * 选择标签
   */
  function selectTag(tagId: string, tagName: string) {
    store.setSidebarActiveSection('tags')
    store.setFilter({ tagId })
    console.log('🏷️ 选择标签:', tagName)
  }

  /**
   * 选择"所有媒体"
   */
  function selectAllMedia() {
    store.setSidebarActiveSection('all')
    store.setFilter({})
    console.log('📷 显示所有媒体')
  }

  /**
   * 添加新标签（占位）
   */
  function addNewTag() {
    console.log('➕ 添加新标签')
    // TODO: 实现添加标签逻辑
  }

  // 组件加载时自动获取文件夹树
  onMounted(() => {
    loadFolderTree()
  })

  return {
    // 状态
    folders,
    tags,
    sidebar,
    selectedFolder,
    selectedTag,
    loading,
    error,

    // 操作
    loadFolderTree,
    toggleFolderExpanded,
    isFolderExpanded,
    selectFolder,
    selectTag,
    selectAllMedia,
    addNewTag,
  }
}
