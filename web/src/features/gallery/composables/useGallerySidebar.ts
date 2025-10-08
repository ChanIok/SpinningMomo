import { ref, computed } from 'vue'
import { useGalleryStore } from '../store'

// 临时类型定义，未来可以移到 types.ts
export interface Folder {
  id: string
  name: string
  count: number
  children?: Folder[]
}

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
  const expandedFolders = ref<Set<string>>(new Set(['root']))

  // 模拟数据（未来从后端获取）
  const folders = ref<Folder[]>([
    {
      id: 'root',
      name: '所有文件夹',
      count: 0,
      children: [
        { id: 'folder-1', name: '我的图片', count: 0 },
        { id: 'folder-2', name: '截图', count: 0 },
      ],
    },
  ])

  const tags = ref<Tag[]>([
    { id: 'tag-1', name: '收藏', count: 0 },
    { id: 'tag-2', name: '重要', count: 0 },
  ])

  // ============= 计算属性 =============

  const sidebar = computed(() => store.sidebar)
  const selectedFolder = computed(() =>
    sidebar.value.activeSection === 'folders' ? store.filter.folderId : null
  )
  const selectedTag = computed(() =>
    sidebar.value.activeSection === 'tags' ? store.filter.tagId : null
  )

  // ============= 操作方法 =============

  /**
   * 切换文件夹展开/收起
   */
  function toggleFolderExpanded(folderId: string) {
    if (expandedFolders.value.has(folderId)) {
      expandedFolders.value.delete(folderId)
    } else {
      expandedFolders.value.add(folderId)
    }
  }

  /**
   * 检查文件夹是否展开
   */
  function isFolderExpanded(folderId: string): boolean {
    return expandedFolders.value.has(folderId)
  }

  /**
   * 选择文件夹
   */
  function selectFolder(folderId: string, folderName: string) {
    store.setSidebarActiveSection('folders')
    store.setFilter({ folderId })
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

  return {
    // 状态
    folders,
    tags,
    sidebar,
    selectedFolder,
    selectedTag,

    // 操作
    toggleFolderExpanded,
    isFolderExpanded,
    selectFolder,
    selectTag,
    selectAllMedia,
    addNewTag,
  }
}
