import { ref, computed } from 'vue'
import { useGalleryStore } from '../store'
import { galleryApi } from '../api'
import type { FolderTreeNode, TagTreeNode } from '../types'

/**
 * Gallery 侧边栏管理 Composable
 * 管理侧边栏UI交互逻辑（展开/收起、选择等）
 * 数据获取由 useGalleryData 负责
 */
export function useGallerySidebar() {
  const store = useGalleryStore()

  // ============= 本地 UI 状态 =============

  // 文件夹展开状态（纯 UI 状态）
  const expandedFolders = ref<Set<number>>(new Set())

  // 标签展开状态（纯 UI 状态）
  const expandedTags = ref<Set<number>>(new Set())

  // ============= 计算属性 =============

  // 从 store 读取文件夹树数据
  const folders = computed(() => store.folders)
  const foldersLoading = computed(() => store.foldersLoading)
  const foldersError = computed(() => store.foldersError)

  // 从 store 读取标签树数据
  const tags = computed(() => store.tags)
  const tagsLoading = computed(() => store.tagsLoading)
  const tagsError = computed(() => store.tagsError)

  const sidebar = computed(() => store.sidebar)
  const selectedFolder = computed(() => {
    if (sidebar.value.activeSection === 'folders' && store.filter.folderId) {
      const folderId = Number(store.filter.folderId)
      return isNaN(folderId) ? null : folderId
    }
    return null
  })
  const selectedTag = computed(() => {
    if (
      sidebar.value.activeSection === 'tags' &&
      store.filter.tagIds &&
      store.filter.tagIds.length > 0
    ) {
      return store.filter.tagIds[0] ?? null
    }
    return null
  })

  // ============= 工具函数 =============

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

  /**
   * 递归查找标签节点
   */
  function findTagById(tags: TagTreeNode[], id: number): TagTreeNode | null {
    for (const tag of tags) {
      if (tag.id === id) return tag
      if (tag.children) {
        const found = findTagById(tag.children, id)
        if (found) return found
      }
    }
    return null
  }

  // ============= UI 交互操作方法 =============

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

    // 查找文件夹对象并设置详情面板
    const folder = findFolderById(store.folders, folderId)
    if (folder) {
      store.setDetailsFocus({ type: 'folder', folder })
    }

    console.log('📁 选择文件夹:', folderName)
  }

  /**
   * 切换标签展开/收起
   */
  function toggleTagExpanded(tagId: number) {
    if (expandedTags.value.has(tagId)) {
      expandedTags.value.delete(tagId)
    } else {
      expandedTags.value.add(tagId)
    }
  }

  /**
   * 检查标签是否展开
   */
  function isTagExpanded(tagId: number): boolean {
    return expandedTags.value.has(tagId)
  }

  /**
   * 选择标签
   */
  function selectTag(tagId: number, tagName: string) {
    // 检查是否点击了当前已选中的标签
    if (selectedTag.value === tagId) {
      store.setFilter({ tagIds: [], tagMatchMode: 'any' })
      console.log('🏷️ 取消标签筛选:', tagName)
    } else {
      // 选中新标签
      store.setSidebarActiveSection('tags')
      store.setFilter({ tagIds: [tagId], tagMatchMode: 'any' })

      // 查找标签对象并设置详情面板
      const tag = findTagById(store.tags, tagId)
      if (tag) {
        store.setDetailsFocus({ type: 'tag', tag })
      }

      console.log('🏷️ 选择标签:', tagName)
    }
  }

  /**
   * 选择"所有媒体"
   */
  function selectAllMedia() {
    store.setSidebarActiveSection('all')
    store.setFilter({})

    // 清除详情面板焦点
    store.clearDetailsFocus()

    console.log('📷 显示所有媒体')
  }

  /**
   * 加载标签树
   */
  async function loadTagTree() {
    try {
      store.setTagsLoading(true)
      store.setTagsError(null)

      const tagTree = await galleryApi.getTagTree()
      store.setTags(tagTree)
    } catch (error) {
      console.error('Failed to load tag tree:', error)
      store.setTagsError('加载标签树失败')
      throw error
    } finally {
      store.setTagsLoading(false)
    }
  }

  /**
   * 添加新标签（占位）
   */
  function addNewTag() {
    console.log('➕ 添加新标签')
    // TODO: 实现添加标签逻辑
  }

  return {
    // 状态（从 store 读取）
    folders,
    foldersLoading,
    foldersError,
    tags,
    tagsLoading,
    tagsError,
    sidebar,
    selectedFolder,
    selectedTag,

    // UI 交互操作
    toggleFolderExpanded,
    isFolderExpanded,
    toggleTagExpanded,
    isTagExpanded,
    selectFolder,
    selectTag,
    selectAllMedia,
    loadTagTree,
    addNewTag,
  }
}
