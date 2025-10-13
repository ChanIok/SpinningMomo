import { ref, computed } from 'vue'
import { useGalleryStore } from '../store'

export interface Tag {
  id: string
  name: string
  count: number
}

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

  // 标签数据（TODO: 未来从 API 获取）
  const tags = ref<Tag[]>([
    { id: 'tag-1', name: '收藏', count: 0 },
    { id: 'tag-2', name: '重要', count: 0 },
  ])

  // ============= 计算属性 =============

  // 从 store 读取文件夹树数据
  const folders = computed(() => store.folders)
  const loading = computed(() => store.foldersLoading)
  const error = computed(() => store.foldersError)

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

    // 设置详情面板显示文件夹
    store.setDetailsFocus({ type: 'folder', folderId })

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

    // 清除详情面板焦点
    store.clearDetailsFocus()

    console.log('📷 显示所有媒体')
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
    loading,
    error,
    tags,
    sidebar,
    selectedFolder,
    selectedTag,

    // UI 交互操作
    toggleFolderExpanded,
    isFolderExpanded,
    selectFolder,
    selectTag,
    selectAllMedia,
    addNewTag,
  }
}
