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

  // 聚合视图收到文件 drop 时提示用户先建立唯一目标。
  function warnDropTargetRequired() {
    toast.warning(t('gallery.drop.selectFolderTitle'), {
      description: t('gallery.drop.selectFolderDescription'),
    })
  }

  // 提交一批 WebView2 File，并把后端批次统计转换成统一用户反馈。
  async function importDroppedFilesToFolder(folderId: number, files: File[]) {
    try {
      const result = await galleryApi.importDroppedFilesToFolder(folderId, files)
      const imported = result.affectedCount ?? 0
      const failed = result.failedCount ?? 0
      const notFound = result.notFoundCount ?? 0
      const skipped = result.unchangedCount ?? 0

      // 全部成功时只报告实际进入图库的文件数。
      if (imported > 0 && failed === 0 && notFound === 0 && skipped === 0) {
        toast.success(t('gallery.drop.successTitle'), {
          description: t('gallery.drop.successDescription', { count: imported }),
        })
        return
      }

      // 只要有成功项就保留正向结果，同时展示其余项目的分类统计。
      if (imported > 0) {
        toast.warning(t('gallery.drop.partialTitle'), {
          description: t('gallery.drop.partialDescription', {
            imported,
            failed,
            notFound,
            skipped,
          }),
        })
        return
      }

      // 全部是不支持媒体或原地文件时属于可解释的跳过，不显示错误。
      if (failed === 0 && notFound === 0 && skipped > 0) {
        toast.warning(t('gallery.drop.skippedTitle'), {
          description: t('gallery.drop.skippedDescription', { count: skipped }),
        })
        return
      }

      toast.error(t('gallery.drop.failedTitle'), {
        description: t('gallery.drop.failedDescription', {
          failed,
          notFound,
          skipped,
        }),
      })
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error)
      toast.error(t('gallery.drop.failedTitle'), { description: message })
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
    importDroppedFilesToFolder,
    warnDropTargetRequired,
  }
}
