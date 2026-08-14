import { computed, reactive, readonly, ref } from 'vue'
import type { Tag } from '../types'
import type { GalleryDeleteMode } from './persistence'

type GalleryContextMenuTarget = 'asset' | 'background'

interface GalleryContextMenuState {
  isOpen: boolean
  requestToken: number
  anchorX: number
  anchorY: number
  target: GalleryContextMenuTarget
}

interface DeleteAssetsDialogState {
  open: boolean
  ids: number[]
  mode: GalleryDeleteMode
  recycleBinCount: number
  permanentCount: number
  unknownCount: number
}

interface GalleryTagClipboardState {
  tagIds: number[]
}

export function createUiSlice() {
  const contextMenuState = reactive<GalleryContextMenuState>({
    isOpen: false,
    requestToken: 0,
    anchorX: 0,
    anchorY: 0,
    target: 'background',
  })
  const moveToFolderDialogOpen = ref(false)
  const preferencesDialogOpen = ref(false)
  const deleteAssetsDialog = reactive<DeleteAssetsDialogState>({
    open: false,
    ids: [],
    mode: 'recycleBin',
    recycleBinCount: 0,
    permanentCount: 0,
    unknownCount: 0,
  })
  const tagClipboard = reactive<GalleryTagClipboardState>({
    tagIds: [],
  })

  const hasTagClipboard = computed(() => tagClipboard.tagIds.length > 0)

  function requestContextMenuOpen(event: MouseEvent) {
    event.preventDefault()
    event.stopPropagation()
    contextMenuState.anchorX = event.clientX
    contextMenuState.anchorY = event.clientY
    // 已开状态下先关闭，让宿主在下一拍基于新锚点“重开”，避免位置不刷新。
    if (contextMenuState.isOpen) {
      contextMenuState.isOpen = false
    }
    // token 仅作为“定位后重开”的信号，不承载业务状态。
    contextMenuState.requestToken += 1
  }

  function openContextMenuForAsset(event: MouseEvent) {
    contextMenuState.target = 'asset'
    requestContextMenuOpen(event)
  }

  function openContextMenuForBackground(event: MouseEvent) {
    contextMenuState.target = 'background'
    requestContextMenuOpen(event)
  }

  function setContextMenuOpen(open: boolean) {
    contextMenuState.isOpen = open
  }

  function setMoveToFolderDialogOpen(open: boolean) {
    moveToFolderDialogOpen.value = open
  }

  function setPreferencesDialogOpen(open: boolean) {
    preferencesDialogOpen.value = open
  }

  function openDeleteAssetsDialog(options: Omit<DeleteAssetsDialogState, 'open'>) {
    Object.assign(deleteAssetsDialog, options, { open: true })
  }

  function setDeleteAssetsDialogOpen(open: boolean) {
    deleteAssetsDialog.open = open
    if (!open) {
      deleteAssetsDialog.ids = []
    }
  }

  function setTagClipboardFromTags(tags: Pick<Tag, 'id' | 'name'>[]) {
    const seenTagIds = new Set<number>()
    // 复制标签时保持原顺序，同时裁掉无效/重复标签，避免后续粘贴重复请求。
    const normalizedTags = tags.filter((tag) => {
      if (tag.id <= 0 || seenTagIds.has(tag.id)) {
        return false
      }

      seenTagIds.add(tag.id)
      return true
    })

    tagClipboard.tagIds = normalizedTags.map((tag) => tag.id)
  }

  function clearTagClipboard() {
    tagClipboard.tagIds = []
  }

  function resetUiState() {
    contextMenuState.isOpen = false
    contextMenuState.requestToken = 0
    contextMenuState.anchorX = 0
    contextMenuState.anchorY = 0
    contextMenuState.target = 'background'
    moveToFolderDialogOpen.value = false
    preferencesDialogOpen.value = false
    deleteAssetsDialog.open = false
    deleteAssetsDialog.ids = []
    deleteAssetsDialog.mode = 'recycleBin'
    deleteAssetsDialog.recycleBinCount = 0
    deleteAssetsDialog.permanentCount = 0
    deleteAssetsDialog.unknownCount = 0
    clearTagClipboard()
  }

  return {
    contextMenu: readonly(contextMenuState),
    openContextMenuForAsset,
    openContextMenuForBackground,
    setContextMenuOpen,
    moveToFolderDialogOpen,
    setMoveToFolderDialogOpen,
    preferencesDialogOpen,
    setPreferencesDialogOpen,
    deleteAssetsDialog: readonly(deleteAssetsDialog),
    openDeleteAssetsDialog,
    setDeleteAssetsDialogOpen,
    tagClipboard: readonly(tagClipboard),
    hasTagClipboard,
    setTagClipboardFromTags,
    clearTagClipboard,
    resetUiState,
  }
}
