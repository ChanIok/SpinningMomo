import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { galleryApi } from '../api'
import { useGalleryData } from './useGalleryData'
import { useGallerySelection } from './useGallerySelection'
import { useGalleryStore } from '../store'
import type { ReviewFlag } from '../types'

export function useGalleryAssetActions() {
  const store = useGalleryStore()
  const galleryData = useGalleryData()
  const gallerySelection = useGallerySelection()
  const { t } = useI18n()
  const { toast } = useToast()

  const selectedAssetIds = computed(() => Array.from(store.selection.selectedIds))
  const isSingleSelection = computed(() => selectedAssetIds.value.length === 1)
  const selectedAssetId = computed(() => {
    if (!isSingleSelection.value) {
      return undefined
    }

    return selectedAssetIds.value[0]
  })

  async function reloadGalleryData() {
    if (store.isTimelineMode) {
      await galleryData.loadTimelineData()
      return
    }

    await galleryData.loadAllAssets()
  }

  async function restoreStateAfterRemoval(previousActiveIndex?: number) {
    await reloadGalleryData()

    if (store.totalCount <= 0) {
      if (store.lightbox.isOpen) {
        store.closeLightbox()
      }
      store.clearActiveAsset()
      gallerySelection.clearSelection()
      return
    }

    const nextActiveIndex = Math.min(previousActiveIndex ?? 0, store.totalCount - 1)

    if (store.lightbox.isOpen) {
      await gallerySelection.selectOnlyIndex(nextActiveIndex)
      return
    }

    await gallerySelection.activateIndex(nextActiveIndex)
    gallerySelection.clearSelection()
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

  async function handleMoveAssetsToTrash() {
    if (selectedAssetIds.value.length === 0) {
      return
    }

    const ids = [...selectedAssetIds.value]
    const previousActiveIndex = store.selection.activeIndex

    try {
      const result = await galleryApi.moveAssetsToTrash(ids)
      const affectedCount = result.affectedCount ?? 0
      if (!result.success && affectedCount === 0) {
        throw new Error(result.message)
      }

      await restoreStateAfterRemoval(previousActiveIndex)

      if (result.success) {
        toast.success(t('gallery.contextMenu.moveToTrash.successTitle'), {
          description: t('gallery.contextMenu.moveToTrash.successDescription', {
            count: affectedCount || ids.length,
          }),
        })
      } else {
        toast.warning(t('gallery.contextMenu.moveToTrash.partialTitle'), {
          description: result.message,
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

  return {
    selectedAssetIds,
    isSingleSelection,
    selectedAssetId,
    handleOpenAssetDefault,
    handleRevealAssetInExplorer,
    handleMoveAssetsToTrash,
    updateSelectedAssetsReviewState,
    setSelectedAssetsRating,
    clearSelectedAssetsRating,
    setSelectedAssetsReviewFlag,
    clearSelectedAssetsReviewFlag,
  }
}
