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
  const ROOT_FOLDER_ID = -1
  const ROOT_TAG_ID = -1

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

  const selectedFolder = computed(() => {
    if (store.filter.folderId) {
      const folderId = Number(store.filter.folderId)
      return isNaN(folderId) ? null : folderId
    }
    return null
  })
  const selectedTag = computed(() => {
    if (store.filter.tagIds && store.filter.tagIds.length > 0) {
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
    store.setFilter({ folderId: String(folderId) })

    // 查找文件夹对象并设置详情面板
    const folder = findFolderById(store.folders, folderId)
    if (folder) {
      store.setDetailsFocus({ type: 'folder', folder })
    }

    console.log('📁 选择文件夹:', folderName)
  }

  /**
   * 清空文件夹筛选（保留其他筛选）
   */
  function clearFolderFilter() {
    store.setFilter({ folderId: undefined })
    store.setDetailsFocus({
      type: 'folder',
      folder: {
        id: ROOT_FOLDER_ID,
        path: '',
        parentId: undefined,
        name: '__root__',
        displayName: '__root__',
        coverAssetId: undefined,
        sortOrder: 0,
        isHidden: false,
        createdAt: 0,
        updatedAt: 0,
        assetCount: store.foldersAssetTotalCount,
        children: [],
      },
    })
    console.log('📁 清空文件夹筛选')
  }

  /**
   * 更新文件夹显示名称（仅应用内）
   */
  async function updateFolderDisplayName(id: number, displayName: string) {
    try {
      await galleryApi.updateFolderDisplayName({
        id,
        displayName,
      })
      console.log('✏️ 更新文件夹显示名称:', id)
    } catch (error) {
      console.error('Failed to update folder display name:', error)
      throw error
    }
  }

  /**
   * 在资源管理器中打开文件夹
   */
  async function openFolderInExplorer(id: number) {
    try {
      await galleryApi.openFolderInExplorer(id)
      console.log('📂 在资源管理器中打开文件夹:', id)
    } catch (error) {
      console.error('Failed to open folder in explorer:', error)
      throw error
    }
  }

  /**
   * 移出根文件夹监听并清理索引
   */
  async function removeFolderWatch(id: number) {
    try {
      const result = await galleryApi.removeFolderWatch(id)
      console.log('🗑️ 移出文件夹监听:', id)
      return result
    } catch (error) {
      console.error('Failed to remove folder watch:', error)
      throw error
    }
  }

  /**
   * 清空标签筛选（保留其他筛选）
   */
  function clearTagFilter() {
    store.setFilter({ tagIds: [], tagMatchMode: 'any' })
    store.setDetailsFocus({
      type: 'tag',
      tag: {
        id: ROOT_TAG_ID,
        name: '__root__',
        parentId: undefined,
        sortOrder: 0,
        createdAt: 0,
        updatedAt: 0,
        assetCount: store.tagsAssetTotalCount,
        children: [],
      },
    })
    console.log('🏷️ 清空标签筛选')
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
    store.resetFilter()

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
   * 创建标签
   */
  async function createTag(name: string, parentId?: number) {
    try {
      console.log('➕ 创建标签:', name, parentId ? `(父标签ID: ${parentId})` : '')

      const result = await galleryApi.createTag({
        name,
        parentId,
      })

      // 重新加载标签树
      await loadTagTree()

      console.log('✅ 标签创建成功:', result.id)
      return result.id
    } catch (error) {
      console.error('Failed to create tag:', error)
      throw error
    }
  }

  /**
   * 更新标签
   */
  async function updateTag(id: number, name: string) {
    try {
      console.log('✏️ 更新标签:', id, name)

      await galleryApi.updateTag({
        id,
        name,
      })

      // 重新加载标签树
      await loadTagTree()

      console.log('✅ 标签更新成功')
    } catch (error) {
      console.error('Failed to update tag:', error)
      throw error
    }
  }

  /**
   * 删除标签
   */
  async function deleteTag(id: number) {
    try {
      console.log('🗑️ 删除标签:', id)

      await galleryApi.deleteTag(id)

      // 重新加载标签树
      await loadTagTree()

      // 如果删除的是当前选中的标签，清除筛选
      if (selectedTag.value === id) {
        store.setFilter({ tagIds: [], tagMatchMode: 'any' })
        store.clearDetailsFocus()
      }

      console.log('✅ 标签删除成功')
    } catch (error) {
      console.error('Failed to delete tag:', error)
      throw error
    }
  }

  return {
    // 状态（从 store 读取）
    folders,
    foldersLoading,
    foldersError,
    tags,
    tagsLoading,
    tagsError,
    selectedFolder,
    selectedTag,

    // UI 交互操作
    toggleFolderExpanded,
    isFolderExpanded,
    toggleTagExpanded,
    isTagExpanded,
    selectFolder,
    clearFolderFilter,
    updateFolderDisplayName,
    openFolderInExplorer,
    removeFolderWatch,
    clearTagFilter,
    selectTag,
    selectAllMedia,
    loadTagTree,
    createTag,
    updateTag,
    deleteTag,
  }
}
