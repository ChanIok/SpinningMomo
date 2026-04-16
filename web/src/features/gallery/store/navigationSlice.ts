import { ref, computed, type Ref } from 'vue'
import type {
  ViewConfig,
  AssetFilter,
  SortBy,
  SortOrder,
  FolderTreeNode,
  TagTreeNode,
} from '../types'
import { collectTreeIds } from './persistence'

interface NavigationSliceArgs {
  // 由入口注入持久化容器，slice 内只消费，不关心 useStorage 初始化细节。
  persistedViewSize: Ref<number>
  persistedViewMode: Ref<ViewConfig['mode']>
  persistedExpandedFolderIds: Ref<number[]>
  persistedExpandedTagIds: Ref<number[]>
}

/**
 * Navigation Slice
 *
 * 关注点:
 * - 左侧导航相关状态（folder/tag tree + expanded）
 * - 查询维度（filter/sort/includeSubfolders）
 * - 视图配置（view mode / size）
 */
export function createNavigationSlice(args: NavigationSliceArgs) {
  const {
    persistedViewSize,
    persistedViewMode,
    persistedExpandedFolderIds,
    persistedExpandedTagIds,
  } = args

  // 导航数据树（由 useGalleryData/useGallerySidebar 驱动加载）。
  const folders = ref<FolderTreeNode[]>([])
  const foldersLoading = ref(false)
  const foldersError = ref<string | null>(null)

  const tags = ref<TagTreeNode[]>([])
  const tagsLoading = ref(false)
  const tagsError = ref<string | null>(null)

  const viewConfig = ref<ViewConfig>({
    mode: persistedViewMode.value,
    size: persistedViewSize.value,
  })
  // 这里的 filter 是“查询输入”，不是 UI 临时态。
  const filter = ref<AssetFilter>({})
  const sortBy = ref<SortBy>('createdAt')
  const sortOrder = ref<SortOrder>('desc')
  const includeSubfolders = ref(true)

  // 便于侧边栏“全部文件夹/全部标签”节点展示统计。
  const foldersAssetTotalCount = computed(() => {
    return folders.value.reduce((sum, folder) => sum + folder.assetCount, 0)
  })

  const tagsAssetTotalCount = computed(() => {
    return tags.value.reduce((sum, tag) => sum + tag.assetCount, 0)
  })

  // 读取层用 Set 提升查询效率，持久化层仍保持数组便于序列化。
  const expandedFolderIdSet = computed(() => new Set(persistedExpandedFolderIds.value))
  const expandedTagIdSet = computed(() => new Set(persistedExpandedTagIds.value))

  function setFolderExpanded(folderId: number, expanded: boolean) {
    const nextExpandedIds = new Set(persistedExpandedFolderIds.value)
    if (expanded) {
      nextExpandedIds.add(folderId)
    } else {
      nextExpandedIds.delete(folderId)
    }
    persistedExpandedFolderIds.value = [...nextExpandedIds]
  }

  function toggleFolderExpanded(folderId: number) {
    setFolderExpanded(folderId, !isFolderExpanded(folderId))
  }

  function isFolderExpanded(folderId: number): boolean {
    return expandedFolderIdSet.value.has(folderId)
  }

  function setTagExpanded(tagId: number, expanded: boolean) {
    const nextExpandedIds = new Set(persistedExpandedTagIds.value)
    if (expanded) {
      nextExpandedIds.add(tagId)
    } else {
      nextExpandedIds.delete(tagId)
    }
    persistedExpandedTagIds.value = [...nextExpandedIds]
  }

  function toggleTagExpanded(tagId: number) {
    setTagExpanded(tagId, !isTagExpanded(tagId))
  }

  function isTagExpanded(tagId: number): boolean {
    return expandedTagIdSet.value.has(tagId)
  }

  function setFolders(newFolders: FolderTreeNode[]) {
    folders.value = newFolders

    // 树重载后把已不存在的节点 id 裁掉，避免 localStorage 越积越脏。
    const validFolderIds = new Set(collectTreeIds(newFolders))
    persistedExpandedFolderIds.value = persistedExpandedFolderIds.value.filter((id) =>
      validFolderIds.has(id)
    )
  }

  function setFoldersLoading(loading: boolean) {
    foldersLoading.value = loading
  }

  function setFoldersError(errorMessage: string | null) {
    foldersError.value = errorMessage
  }

  function setTags(newTags: TagTreeNode[]) {
    tags.value = newTags

    // 标签树和文件夹树一样，刷新后同步清理失效展开状态。
    const validTagIds = new Set(collectTreeIds(newTags))
    persistedExpandedTagIds.value = persistedExpandedTagIds.value.filter((id) =>
      validTagIds.has(id)
    )
  }

  function setTagsLoading(loading: boolean) {
    tagsLoading.value = loading
  }

  function setTagsError(errorMessage: string | null) {
    tagsError.value = errorMessage
  }

  function setViewConfig(config: Partial<ViewConfig>) {
    const merged = { ...viewConfig.value, ...config }
    viewConfig.value = merged
    persistedViewSize.value = viewConfig.value.size
    persistedViewMode.value = viewConfig.value.mode
  }

  function setFilter(newFilter: Partial<AssetFilter>) {
    // 使用 merge 而不是覆盖，允许 composable 按字段增量更新筛选项。
    filter.value = { ...filter.value, ...newFilter }
  }

  function resetFilter() {
    filter.value = {}
  }

  function setSorting(newSortBy: SortBy, newSortOrder: SortOrder) {
    sortBy.value = newSortBy
    sortOrder.value = newSortOrder
  }

  function setIncludeSubfolders(include: boolean) {
    includeSubfolders.value = include
  }

  function resetNavigationState() {
    // 只重置导航/筛选域，不触碰查询缓存与交互态。
    folders.value = []
    foldersLoading.value = false
    foldersError.value = null

    tags.value = []
    tagsLoading.value = false
    tagsError.value = null

    persistedExpandedFolderIds.value = []
    persistedExpandedTagIds.value = []

    viewConfig.value = { mode: 'grid', size: 128 }
    persistedViewSize.value = viewConfig.value.size
    persistedViewMode.value = viewConfig.value.mode
    resetFilter()
    sortBy.value = 'createdAt'
    sortOrder.value = 'desc'
    includeSubfolders.value = true
  }

  return {
    folders,
    foldersLoading,
    foldersError,
    tags,
    tagsLoading,
    tagsError,
    viewConfig,
    filter,
    sortBy,
    sortOrder,
    includeSubfolders,
    foldersAssetTotalCount,
    tagsAssetTotalCount,
    setFolderExpanded,
    toggleFolderExpanded,
    isFolderExpanded,
    setTagExpanded,
    toggleTagExpanded,
    isTagExpanded,
    setFolders,
    setFoldersLoading,
    setFoldersError,
    setTags,
    setTagsLoading,
    setTagsError,
    setViewConfig,
    setFilter,
    resetFilter,
    setSorting,
    setIncludeSubfolders,
    resetNavigationState,
  }
}
