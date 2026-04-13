import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { galleryApi } from '../api'
import { useGalleryStore } from '../store'
import type { ReviewFlag } from '../types'

export function useGalleryAssetActions() {
  const store = useGalleryStore()
  const { t } = useI18n()
  const { toast } = useToast()

  const selectedAssetIds = computed(() => Array.from(store.selection.selectedIds))
  const hasSelection = computed(() => selectedAssetIds.value.length > 0)
  const isSingleSelection = computed(() => selectedAssetIds.value.length === 1)
  const selectedAssetId = computed(() => {
    if (!isSingleSelection.value) {
      return undefined
    }

    return selectedAssetIds.value[0]
  })

  function buildMoveToTrashDescription(result: {
    affectedCount?: number
    failedCount?: number
    notFoundCount?: number
  }) {
    return t('gallery.contextMenu.moveToTrash.partialDescription', {
      moved: result.affectedCount ?? 0,
      failed: result.failedCount ?? 0,
      notFound: result.notFoundCount ?? 0,
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

  async function handleMoveAssetsToTrash() {
    if (selectedAssetIds.value.length === 0) {
      return
    }

    const ids = [...selectedAssetIds.value]
    const previousActiveIndex = store.selection.activeIndex

    try {
      const result = await galleryApi.moveAssetsToTrash(ids)
      const affectedCount = result.affectedCount ?? 0
      const failedCount = result.failedCount ?? 0
      const notFoundCount = result.notFoundCount ?? 0
      if (!result.success && affectedCount === 0) {
        throw new Error(
          t('gallery.contextMenu.moveToTrash.failedDescription', {
            failed: failedCount,
            notFound: notFoundCount,
          })
        )
      }

      // 等待 gallery.changed 统一刷新，先做最小本地修复避免短暂状态错乱。
      const nextSelectedIds = selectedAssetIds.value.filter((id) => !ids.includes(id))
      store.replaceSelection(nextSelectedIds)
      store.setSelectionAnchor(undefined)

      const activeAssetId = store.selection.activeAssetId
      if (activeAssetId !== undefined && ids.includes(activeAssetId)) {
        if (store.lightbox.isOpen) {
          store.closeLightbox()
        }
        store.clearActiveAsset()
        if (previousActiveIndex !== undefined && previousActiveIndex > 0) {
          store.setSelectionActive(previousActiveIndex - 1)
        }
      }

      if (result.success) {
        toast.success(t('gallery.contextMenu.moveToTrash.successTitle'), {
          description: t('gallery.contextMenu.moveToTrash.successDescription', {
            count: affectedCount || ids.length,
          }),
        })
      } else {
        toast.warning(t('gallery.contextMenu.moveToTrash.partialTitle'), {
          description: buildMoveToTrashDescription(result),
        })
      }
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error)
      toast.error(t('gallery.contextMenu.moveToTrash.failedTitle'), {
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

  return {
    selectedAssetIds,
    hasSelection,
    isSingleSelection,
    selectedAssetId,
    handleOpenAssetDefault,
    handleRevealAssetInExplorer,
    handleCopyAssetsToClipboard,
    handleMoveAssetsToTrash,
    updateSelectedAssetsReviewState,
    setSelectedAssetsRating,
    clearSelectedAssetsRating,
    setSelectedAssetsReviewFlag,
    clearSelectedAssetsReviewFlag,
    setSelectedAssetsRejected,
    clearSelectedAssetsRejected,
  }
}
