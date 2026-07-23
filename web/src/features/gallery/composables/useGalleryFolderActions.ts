import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { galleryApi } from '../api'
import { useGalleryStore } from '../store'

export function useGalleryFolderActions() {
  const store = useGalleryStore()
  const { t } = useI18n()
  const { toast } = useToast()

  const selectedFolderId = computed(() => {
    const rawFolderId = store.filter.folderId
    if (!rawFolderId) {
      return undefined
    }
    const folderId = Number(rawFolderId)
    return Number.isInteger(folderId) && folderId > 0 ? folderId : undefined
  })

  // 将当前系统剪贴板内容导入明确的目标文件夹，并按批次结果给出反馈。
  async function pasteClipboardToFolder(folderId: number) {
    try {
      const result = await galleryApi.pasteClipboardToFolder(folderId)
      const pasted = result.affectedCount ?? 0
      const failed = result.failedCount ?? 0
      const notFound = result.notFoundCount ?? 0
      const skipped = result.unchangedCount ?? 0

      if (pasted > 0 && failed === 0 && notFound === 0 && skipped === 0) {
        toast.success(t('gallery.paste.successTitle'), {
          description: t('gallery.paste.successDescription', { count: pasted }),
        })
        return
      }

      if (pasted > 0) {
        toast.warning(t('gallery.paste.partialTitle'), {
          description: t('gallery.paste.partialDescription', {
            pasted,
            failed,
            notFound,
            skipped,
          }),
        })
        return
      }

      if (failed === 0 && notFound === 0 && skipped === 0) {
        toast.warning(t('gallery.paste.emptyTitle'), {
          description: t('gallery.paste.emptyDescription'),
        })
        return
      }

      toast.error(t('gallery.paste.failedTitle'), {
        description: t('gallery.paste.failedDescription', {
          failed,
          notFound,
          skipped,
        }),
      })
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error)
      toast.error(t('gallery.paste.failedTitle'), { description: message })
    }
  }

  // 仅在当前视图绑定了唯一文件夹时响应快捷键，聚合视图要求用户先选目标。
  async function pasteClipboardToSelectedFolder() {
    const folderId = selectedFolderId.value
    if (folderId === undefined) {
      toast.warning(t('gallery.paste.selectFolderTitle'), {
        description: t('gallery.paste.selectFolderDescription'),
      })
      return
    }
    await pasteClipboardToFolder(folderId)
  }

  return {
    selectedFolderId,
    pasteClipboardToFolder,
    pasteClipboardToSelectedFolder,
  }
}
