import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useGalleryStore } from '../store'
import type { FolderTreeNode, SortBy, TagTreeNode, ViewMode } from '../types'
import { Grid3x3, LayoutGrid, List, Rows3 } from '@lucide/vue'

type SourceType = 'all' | 'folder' | 'tag'

function findFolderNameById(nodes: FolderTreeNode[], id: number): string | null {
  for (const node of nodes) {
    if (node.id === id) {
      return node.displayName || node.name
    }
    const childName = findFolderNameById(node.children, id)
    if (childName) {
      return childName
    }
  }
  return null
}

function findTagNameById(nodes: TagTreeNode[], id: number): string | null {
  for (const node of nodes) {
    if (node.id === id) {
      return node.name
    }
    const childName = findTagNameById(node.children, id)
    if (childName) {
      return childName
    }
  }
  return null
}

export function useGalleryViewControls() {
  const { t } = useI18n()
  const store = useGalleryStore()

  const viewModes = [
    { value: 'grid' as ViewMode, icon: Grid3x3, i18nKey: 'gallery.toolbar.viewMode.grid' },
    { value: 'adaptive' as ViewMode, icon: Rows3, i18nKey: 'gallery.toolbar.viewMode.adaptive' },
    { value: 'masonry' as ViewMode, icon: LayoutGrid, i18nKey: 'gallery.toolbar.viewMode.masonry' },
    { value: 'list' as ViewMode, icon: List, i18nKey: 'gallery.toolbar.viewMode.list' },
  ]

  const viewMode = computed(() =>
    store.isCompactWindow && store.view.mode === 'list' ? 'grid' : store.view.mode
  )
  const sortBy = computed(() => store.sortBy)
  const sortOrder = computed(() => store.sortOrder)
  const currentFolderOnly = computed(() => !store.includeSubfolders)
  const currentSliderPosition = computed(() => store.getSliderPosition())
  const availableViewModes = computed(() =>
    store.isCompactWindow ? viewModes.filter((mode) => mode.value !== 'list') : viewModes
  )
  const currentViewModeIcon = computed(() => {
    const mode = availableViewModes.value.find((item) => item.value === viewMode.value)
    return mode?.icon || Grid3x3
  })
  const currentSource = computed<{ type: SourceType; label: string }>(() => {
    const folderId = store.filter.folderId ? Number(store.filter.folderId) : null
    if (folderId !== null && Number.isFinite(folderId)) {
      return {
        type: 'folder',
        label:
          findFolderNameById(store.folders, folderId) ||
          t('gallery.toolbar.browse.folderFallback', { id: folderId }),
      }
    }

    const tagId = store.filter.tagIds?.[0]
    if (tagId !== undefined) {
      return {
        type: 'tag',
        label:
          findTagNameById(store.tags, tagId) ||
          t('gallery.toolbar.browse.tagFallback', { id: tagId }),
      }
    }

    return { type: 'all', label: t('gallery.toolbar.browse.all') }
  })
  const sortOrderLabel = computed(() =>
    sortOrder.value === 'asc'
      ? t('gallery.toolbar.sortOrder.asc')
      : t('gallery.toolbar.sortOrder.desc')
  )

  function onSortByChange(value: string | number | bigint | Record<string, any> | null) {
    if (value) {
      store.setSorting(String(value) as SortBy, sortOrder.value)
    }
  }

  function toggleSortOrder() {
    store.setSorting(sortBy.value, sortOrder.value === 'asc' ? 'desc' : 'asc')
  }

  function onCurrentFolderOnlyChange(value: boolean) {
    store.includeSubfolders = !value
  }

  function setViewMode(
    mode:
      | string
      | number
      | bigint
      | Record<string, any>
      | null
      | (string | number | bigint | Record<string, any> | null)[]
  ) {
    if (mode && typeof mode === 'string') {
      if (store.isCompactWindow && mode === 'list') {
        return
      }
      store.view.mode = mode as ViewMode
    }
  }

  function onViewSizeSliderChange(value: number[] | undefined) {
    if (value && value.length > 0 && value[0] !== undefined) {
      store.setViewSizeFromSlider(value[0])
    }
  }

  return {
    viewModes,
    viewMode,
    sortBy,
    sortOrder,
    currentFolderOnly,
    currentSliderPosition,
    availableViewModes,
    currentViewModeIcon,
    currentSource,
    sortOrderLabel,
    onSortByChange,
    toggleSortOrder,
    onCurrentFolderOnlyChange,
    setViewMode,
    onViewSizeSliderChange,
  }
}
