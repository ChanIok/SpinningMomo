import { computed, reactive, readonly } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { galleryApi } from '../api'
import { useGalleryData } from './useGalleryData'
import { useGalleryTagClipboard } from './useGalleryTagClipboard'
import { useGalleryStore } from '../store'
import type { ReviewFlag } from '../types'
import type { GalleryDeleteMode } from '../store/persistence'

interface MoveToFolderDialogState {
  open: boolean
}

const moveToFolderDialogState = reactive<MoveToFolderDialogState>({
  open: false,
})

interface DeleteAssetsDialogState {
  open: boolean
  ids: number[]
  mode: GalleryDeleteMode
  recycleBinCount: number
  permanentCount: number
  unknownCount: number
}

const deleteAssetsDialogState = reactive<DeleteAssetsDialogState>({
  open: false,
  ids: [],
  mode: 'recycleBin',
  recycleBinCount: 0,
  permanentCount: 0,
  unknownCount: 0,
})

export function useGalleryAssetActions() {
  const store = useGalleryStore()
  const { t } = useI18n()
  const { toast } = useToast()
  const galleryData = useGalleryData()
  const tagClipboard = useGalleryTagClipboard()

  const selectedAssetIds = computed(() => Array.from(store.selection.selectedIds))
  const hasSelection = computed(() => selectedAssetIds.value.length > 0)
  const isSingleSelection = computed(() => selectedAssetIds.value.length === 1)
  const canCopyTags = computed(() => hasSelection.value)
  const canPasteTags = computed(() => hasSelection.value && tagClipboard.hasPayload.value)
  const copiedTagCount = computed(() => tagClipboard.state.tagIds.length)
  const selectedAssetId = computed(() => {
    if (!isSingleSelection.value) {
      return undefined
    }

    return selectedAssetIds.value[0]
  })
  const currentDeleteImpact = computed(() => {
    // 分页 Map 原地更新时依靠显式版本触发菜单语义重算。
    void store.paginatedAssetsVersion
    return getKnownDeleteImpact(selectedAssetIds.value, store.gallerySettings.deletion.mode)
  })
  const deleteMenuLabel = computed(() => {
    if (store.gallerySettings.deletion.mode === 'permanent') {
      return t('gallery.contextMenu.delete.permanentLabel')
    }

    const impact = currentDeleteImpact.value
    if (impact.unknownCount > 0 || (impact.permanentCount > 0 && impact.recycleBinCount > 0)) {
      return t('gallery.contextMenu.delete.genericLabel')
    }
    if (impact.permanentCount > 0) {
      return t('gallery.contextMenu.delete.permanentLabel')
    }
    return t('gallery.contextMenu.delete.recycleBinLabel')
  })

  function buildDeleteDescription(result: {
    affectedCount?: number
    failedCount?: number
    notFoundCount?: number
  }) {
    return t('gallery.contextMenu.delete.partialDescription', {
      deleted: result.affectedCount ?? 0,
      failed: result.failedCount ?? 0,
      notFound: result.notFoundCount ?? 0,
    })
  }

  function buildDeleteSuccessDescription(result: {
    recycleBinCount: number
    permanentCount: number
  }) {
    const recycled = result.recycleBinCount
    const permanent = result.permanentCount

    if (recycled > 0 && permanent > 0) {
      return t('gallery.contextMenu.delete.successDescription', {
        recycled,
        permanent,
      })
    }
    if (recycled > 0) {
      return t('gallery.contextMenu.delete.successRecycleBin', {
        count: recycled,
      })
    }
    if (permanent > 0) {
      return t('gallery.contextMenu.delete.successPermanent', {
        count: permanent,
      })
    }
    return t('gallery.contextMenu.delete.successDescription', {
      recycled,
      permanent,
    })
  }

  function buildCopyFilesDescription(result: {
    affectedCount?: number
    failedCount?: number
    notFoundCount?: number
  }) {
    return t('gallery.contextMenu.copyFiles.partialDescription', {
      copied: result.affectedCount ?? 0,
      failed: result.failedCount ?? 0,
      notFound: result.notFoundCount ?? 0,
    })
  }

  function buildPasteTagsPartialDescription(result: {
    assetCount: number
    tagCount: number
    affected: number
    failed: number
    unchanged: number
  }) {
    return t('gallery.contextMenu.pasteTags.partialDescription', {
      assetCount: result.assetCount,
      tagCount: result.tagCount,
      affected: result.affected,
      failed: result.failed,
      unchanged: result.unchanged,
    })
  }

  function resolvePrimaryTagSourceAssetId() {
    const activeAssetId = store.selection.activeAssetId
    if (activeAssetId !== undefined && store.selection.selectedIds.has(activeAssetId)) {
      return activeAssetId
    }

    if (selectedAssetId.value !== undefined) {
      return selectedAssetId.value
    }

    return selectedAssetIds.value[0]
  }

  async function refreshTagViewsAfterMutation() {
    try {
      // 标签变更会同时影响左侧标签树计数、当前结果集命中、以及右侧详情标签区。
      await Promise.all([
        galleryData.loadTagTree({ silent: true }),
        galleryData.refreshCurrentQuery(),
      ])
    } catch (error) {
      console.error('Failed to refresh gallery after tag mutation:', error)
    } finally {
      store.bumpAssetTagsVersion()
    }
  }

  async function handleOpenAssetDefault() {
    const assetId = selectedAssetId.value
    if (assetId === undefined) {
      return
    }

    try {
      const result = await galleryApi.openAssetDefault(assetId)
      if (!result.success) {
        throw new Error(result.message)
      }
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error)
      toast.error(t('gallery.contextMenu.openDefaultApp.failedTitle'), {
        description: message,
      })
    }
  }

  async function handleRevealAssetInExplorer() {
    const assetId = selectedAssetId.value
    if (assetId === undefined) {
      return
    }

    try {
      const result = await galleryApi.revealAssetInExplorer(assetId)
      if (!result.success) {
        throw new Error(result.message)
      }
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error)
      toast.error(t('gallery.contextMenu.revealInExplorer.failedTitle'), {
        description: message,
      })
    }
  }

  async function handleCopyAssetsToClipboard() {
    if (selectedAssetIds.value.length === 0) {
      return
    }

    try {
      const result = await galleryApi.copyAssetsToClipboard(selectedAssetIds.value)
      const copiedCount = result.affectedCount ?? 0

      if (!result.success && copiedCount === 0) {
        throw new Error(result.message)
      }

      if (result.success) {
        toast.success(t('gallery.contextMenu.copyFiles.successTitle'), {
          description: t('gallery.contextMenu.copyFiles.successDescription', {
            count: copiedCount || selectedAssetIds.value.length,
          }),
        })
      } else {
        toast.warning(t('gallery.contextMenu.copyFiles.partialTitle'), {
          description: buildCopyFilesDescription(result),
        })
      }
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error)
      toast.error(t('gallery.contextMenu.copyFiles.failedTitle'), {
        description: message,
      })
    }
  }

  async function moveAssetsToFolderByIds(folderId: number, assetIds: number[]) {
    const uniqueIds = [...new Set(assetIds)].filter((id) => id > 0)
    if (uniqueIds.length === 0) {
      return
    }

    try {
      const result = await galleryApi.moveAssetsToFolder({
        ids: uniqueIds,
        targetFolderId: folderId,
      })
      const affectedCount = result.affectedCount ?? 0
      const failedCount = result.failedCount ?? 0
      const notFoundCount = result.notFoundCount ?? 0
      const unchangedCount = result.unchangedCount ?? 0
      if (!result.success && affectedCount === 0) {
        throw new Error(
          t('gallery.sidebar.folders.moveAssets.failedDescription', {
            failed: failedCount,
            notFound: notFoundCount,
            unchanged: unchangedCount,
          })
        )
      }

      await Promise.all([
        galleryData.loadFolderTree({ silent: true }),
        galleryData.refreshCurrentQuery(),
      ])
      store.clearSelection()

      if (result.success) {
        toast.success(t('gallery.sidebar.folders.moveAssets.successTitle'), {
          description: t('gallery.sidebar.folders.moveAssets.successDescription', {
            count: affectedCount,
          }),
        })
      } else {
        toast.warning(t('gallery.sidebar.folders.moveAssets.partialTitle'), {
          description: t('gallery.sidebar.folders.moveAssets.partialDescription', {
            moved: affectedCount,
            failed: failedCount,
            notFound: notFoundCount,
            unchanged: unchangedCount,
          }),
        })
      }
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error)
      toast.error(t('gallery.sidebar.folders.moveAssets.failedTitle'), { description: message })
    }
  }

  async function handleMoveSelectedAssetsToFolder(folderId: number) {
    if (selectedAssetIds.value.length === 0) {
      return
    }

    await moveAssetsToFolderByIds(folderId, selectedAssetIds.value)
  }

  function openMoveToFolderDialog() {
    if (!hasSelection.value) {
      return
    }

    moveToFolderDialogState.open = true
  }

  function setMoveToFolderDialogOpen(open: boolean) {
    moveToFolderDialogState.open = open
  }

  function getKnownDeleteImpact(ids: number[], mode: GalleryDeleteMode) {
    if (mode === 'permanent') {
      return { recycleBinCount: 0, permanentCount: ids.length, unknownCount: 0 }
    }

    const selectedIdSet = new Set(ids)
    const knownAssets = new Map<number, { rootId?: number }>()
    store.paginatedAssets.forEach((assets) => {
      for (const asset of assets) {
        if (selectedIdSet.has(asset.id)) {
          knownAssets.set(asset.id, asset)
        }
      }
    })

    let recycleBinCount = 0
    let permanentCount = 0
    let unknownCount = 0
    for (const id of ids) {
      const asset = knownAssets.get(id)
      if (asset?.rootId === undefined) {
        unknownCount++
        continue
      }

      const isNetwork = store.rootNetworkById.get(asset.rootId)
      if (isNetwork === undefined) {
        unknownCount++
      } else if (isNetwork) {
        permanentCount++
      } else {
        recycleBinCount++
      }
    }

    return {
      recycleBinCount,
      permanentCount,
      unknownCount,
    }
  }

  function openDeleteAssetsDialog(options: Omit<DeleteAssetsDialogState, 'open'>) {
    Object.assign(deleteAssetsDialogState, options, { open: true })
  }

  function setDeleteAssetsDialogOpen(open: boolean) {
    deleteAssetsDialogState.open = open
    if (!open) {
      deleteAssetsDialogState.ids = []
    }
  }

  async function executeDeleteAssets(ids: number[], mode: GalleryDeleteMode) {
    const previousActiveAssetId = store.selection.activeAssetId

    try {
      const result = await galleryApi.deleteAssets({
        ids,
        mode: mode === 'permanent' ? 'permanent' : 'recycle_where_possible',
      })

      const affectedCount = result.affectedCount ?? 0
      const failedCount = result.failedCount ?? 0
      const notFoundCount = result.notFoundCount ?? 0
      if (!result.success && affectedCount === 0) {
        throw new Error(
          t('gallery.contextMenu.delete.failedDescription', {
            failed: failedCount,
            notFound: notFoundCount,
          })
        )
      }

      // 先做最小本地修复；当前焦点被删除时主动刷新，由查询协调层重建焦点。
      const nextSelectedIds = selectedAssetIds.value.filter((id) => !ids.includes(id))
      store.replaceSelection(nextSelectedIds)
      store.setSelectionAnchor(undefined)

      if (previousActiveAssetId !== undefined && ids.includes(previousActiveAssetId)) {
        await galleryData.refreshCurrentQuery()
      }

      if (result.success) {
        toast.success(t('gallery.contextMenu.delete.successTitle'), {
          description: buildDeleteSuccessDescription(result),
        })
      } else {
        toast.warning(t('gallery.contextMenu.delete.partialTitle'), {
          description: buildDeleteDescription(result),
        })
      }
      return true
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error)
      toast.error(t('gallery.contextMenu.delete.failedTitle'), {
        description: message,
      })
      return false
    }
  }

  async function requestDeleteAssets() {
    const ids = [...selectedAssetIds.value]
    if (ids.length === 0) {
      return
    }

    const mode = store.gallerySettings.deletion.mode
    const impact = getKnownDeleteImpact(ids, mode)
    if (
      mode === 'permanent' ||
      store.gallerySettings.deletion.confirmRecycleBin ||
      impact.permanentCount > 0 ||
      impact.unknownCount > 0
    ) {
      openDeleteAssetsDialog({
        ids,
        mode,
        ...impact,
      })
      return
    }

    await executeDeleteAssets(ids, mode)
  }

  async function confirmDeleteAssets() {
    const { ids, mode, permanentCount } = deleteAssetsDialogState
    if (ids.length === 0) {
      return false
    }

    const effectiveMode: GalleryDeleteMode =
      mode === 'permanent' || permanentCount > 0 ? 'permanent' : mode

    const completed = await executeDeleteAssets([...ids], effectiveMode)
    if (completed) {
      setDeleteAssetsDialogOpen(false)
    }
    return completed
  }

  async function copySelectedAssetTags() {
    const sourceAssetId = resolvePrimaryTagSourceAssetId()
    if (sourceAssetId === undefined) {
      return
    }

    try {
      const tags = await galleryApi.getAssetTags(sourceAssetId)
      if (tags.length === 0) {
        tagClipboard.clear()
        toast.warning(t('gallery.contextMenu.copyTags.emptyTitle'), {
          description: t('gallery.contextMenu.copyTags.emptyDescription'),
        })
        return
      }

      tagClipboard.setFromTags(sourceAssetId, tags)
      toast.success(t('gallery.contextMenu.copyTags.successTitle'), {
        description: t('gallery.contextMenu.copyTags.successDescription', {
          count: tags.length,
        }),
      })
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error)
      toast.error(t('gallery.contextMenu.copyTags.failedTitle'), {
        description: message,
      })
    }
  }

  async function pasteCopiedTagsToSelection() {
    if (selectedAssetIds.value.length === 0) {
      return
    }

    const tagIds = [...new Set(tagClipboard.state.tagIds)].filter((tagId) => tagId > 0)
    if (tagIds.length === 0) {
      toast.warning(t('gallery.contextMenu.pasteTags.emptyTitle'), {
        description: t('gallery.contextMenu.pasteTags.emptyDescription'),
      })
      return
    }

    const assetIds = [...new Set(selectedAssetIds.value)].filter((assetId) => assetId > 0)
    if (assetIds.length === 0) {
      return
    }

    try {
      if (assetIds.length === 1) {
        const result = await galleryApi.addTagsToAsset({
          assetId: assetIds[0]!,
          tagIds,
        })

        if (!result.success) {
          throw new Error(result.message)
        }

        await refreshTagViewsAfterMutation()
        toast.success(t('gallery.contextMenu.pasteTags.successTitle'), {
          description: t('gallery.contextMenu.pasteTags.successDescription', {
            assetCount: assetIds.length,
            tagCount: tagIds.length,
          }),
        })
        return
      }

      let affectedRelations = 0
      let failedRelations = 0
      let unchangedRelations = 0

      // 现有 RPC 粒度是“一个标签 -> 多个资产”，这里按标签维度批量复用即可。
      for (const tagId of tagIds) {
        const result = await galleryApi.addTagToAssets({
          assetIds,
          tagId,
        })

        if (!result.success && (result.affectedCount ?? 0) === 0) {
          throw new Error(result.message)
        }

        affectedRelations += result.affectedCount ?? 0
        failedRelations += result.failedCount ?? 0
        unchangedRelations += result.unchangedCount ?? 0
      }

      await refreshTagViewsAfterMutation()

      if (failedRelations === 0 && unchangedRelations === 0) {
        toast.success(t('gallery.contextMenu.pasteTags.successTitle'), {
          description: t('gallery.contextMenu.pasteTags.successDescription', {
            assetCount: assetIds.length,
            tagCount: tagIds.length,
          }),
        })
        return
      }

      toast.warning(t('gallery.contextMenu.pasteTags.partialTitle'), {
        description: buildPasteTagsPartialDescription({
          assetCount: assetIds.length,
          tagCount: tagIds.length,
          affected: affectedRelations,
          failed: failedRelations,
          unchanged: unchangedRelations,
        }),
      })
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error)
      toast.error(t('gallery.contextMenu.pasteTags.failedTitle'), {
        description: message,
      })
    }
  }

  async function updateSelectedAssetsReviewState(payload: {
    rating?: number
    reviewFlag?: ReviewFlag
  }) {
    if (selectedAssetIds.value.length === 0) {
      return
    }

    try {
      const result = await galleryApi.updateAssetsReviewState({
        assetIds: selectedAssetIds.value,
        rating: payload.rating,
        reviewFlag: payload.reviewFlag,
      })

      if (!result.success) {
        throw new Error(result.message)
      }

      // 审片是高频操作：成功后优先 patch 当前已加载数据，避免等待整页重载。
      store.patchAssetsReviewState(selectedAssetIds.value, payload)
      store.patchBatchSummaryReviewState(payload)
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error)
      toast.error(t('gallery.review.update.failedTitle'), {
        description: message,
      })
      throw error
    }
  }

  async function setSelectedAssetsRating(rating: number) {
    await updateSelectedAssetsReviewState({ rating })
  }

  async function clearSelectedAssetsRating() {
    await updateSelectedAssetsReviewState({ rating: 0 })
  }

  async function updateSelectedAssetsDescription(description?: string) {
    const assetIds = [...new Set(selectedAssetIds.value)].filter((assetId) => assetId > 0)
    if (assetIds.length === 0) {
      return
    }

    try {
      const result = await galleryApi.updateAssetsDescription({
        assetIds,
        description,
      })

      if (!result.success) {
        throw new Error(result.message)
      }

      store.patchAssetsDescription(assetIds, description)
      store.patchBatchSummaryDescription(description)
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error)
      toast.error(t('gallery.details.asset.updateDescriptionFailed'), {
        description: message,
      })
      throw error
    }
  }

  async function setSelectedAssetsReviewFlag(reviewFlag: ReviewFlag) {
    await updateSelectedAssetsReviewState({ reviewFlag })
  }

  async function clearSelectedAssetsReviewFlag() {
    await updateSelectedAssetsReviewState({ reviewFlag: 'none' })
  }

  async function setSelectedAssetsRejected() {
    await setSelectedAssetsReviewFlag('rejected')
  }

  async function clearSelectedAssetsRejected() {
    await clearSelectedAssetsReviewFlag()
  }

  async function addTagToSelectedAssets(tagId: number) {
    const assetIds = [...new Set(selectedAssetIds.value)].filter((assetId) => assetId > 0)
    if (assetIds.length === 0) {
      return
    }

    const result = await galleryApi.addTagToAssets({
      assetIds,
      tagId,
    })

    if (!result.success && (result.affectedCount ?? 0) === 0) {
      throw new Error(result.message)
    }

    await refreshTagViewsAfterMutation()
  }

  async function removeTagFromSelectedAssets(tagId: number) {
    const assetIds = [...new Set(selectedAssetIds.value)].filter((assetId) => assetId > 0)
    if (assetIds.length === 0) {
      return
    }

    const result = await galleryApi.removeTagFromAssets({
      assetIds,
      tagId,
    })

    if (!result.success && (result.affectedCount ?? 0) === 0) {
      throw new Error(result.message)
    }

    await refreshTagViewsAfterMutation()
  }

  return {
    selectedAssetIds,
    hasSelection,
    isSingleSelection,
    canCopyTags,
    canPasteTags,
    copiedTagCount,
    selectedAssetId,
    deleteMenuLabel,
    moveToFolderDialog: readonly(moveToFolderDialogState),
    openMoveToFolderDialog,
    setMoveToFolderDialogOpen,
    moveAssetsToFolderByIds,
    handleMoveSelectedAssetsToFolder,
    handleOpenAssetDefault,
    handleRevealAssetInExplorer,
    handleCopyAssetsToClipboard,
    deleteAssetsDialog: readonly(deleteAssetsDialogState),
    setDeleteAssetsDialogOpen,
    requestDeleteAssets,
    confirmDeleteAssets,
    copySelectedAssetTags,
    pasteCopiedTagsToSelection,
    updateSelectedAssetsReviewState,
    setSelectedAssetsRating,
    clearSelectedAssetsRating,
    updateSelectedAssetsDescription,
    setSelectedAssetsReviewFlag,
    clearSelectedAssetsReviewFlag,
    setSelectedAssetsRejected,
    clearSelectedAssetsRejected,
    addTagToSelectedAssets,
    removeTagFromSelectedAssets,
  }
}
