import { useAssetsStore } from '@/lib/assets/assetsStore'
import { Separator } from '@/components/ui/separator'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import { X, FileImage, Calendar, HardDrive, Ruler } from 'lucide-react'
import { formatBytes } from '@/lib/utils'
import { useTranslation } from '@/lib/i18n'

export function GalleryDetails() {
  const { assets, selection, setActiveAsset } = useAssetsStore()
  const { t } = useTranslation()

  // 获取当前激活的资产
  const activeAsset = selection.activeId ? assets.find((a) => a.id === selection.activeId) : null

  // 获取选中的资产数量
  const selectedCount = selection.selectedIds.size

  if (!activeAsset && selectedCount === 0) {
    return (
      <div className='flex h-full items-center justify-center p-4'>
        <div className='text-center text-muted-foreground'>
          <FileImage className='mx-auto mb-4 h-12 w-12 opacity-50' />
          <p className='text-sm'>{t('gallery.details.selectToView')}</p>
        </div>
      </div>
    )
  }

  // 如果选中多个资产，显示批量操作界面
  if (selectedCount > 1) {
    return (
      <div className='p-4'>
        <div className='mb-4 flex items-center justify-between'>
          <h3 className='font-medium'>{t('gallery.details.batchOperations')}</h3>
          <Button
            variant='ghost'
            size='icon'
            onClick={() => useAssetsStore.getState().clearSelection()}
          >
            <X className='h-4 w-4' />
          </Button>
        </div>

        <div className='space-y-4'>
          <div className='text-sm text-muted-foreground'>
            {t('gallery.details.selectedCount', { count: selectedCount })}
          </div>

          <Separator />

          <div className='space-y-2'>
            <p className='text-sm font-medium'>批量操作</p>
            <div className='text-xs text-muted-foreground'>敬请期待...</div>
          </div>
        </div>
      </div>
    )
  }

  // 显示单个资产的详细信息
  if (!activeAsset) return null

  return (
    <div className='p-4'>
      <div className='mb-4 flex items-center justify-between'>
        <h3 className='font-medium'>{t('gallery.details.title')}</h3>
        <Button variant='ghost' size='icon' onClick={() => setActiveAsset(undefined)}>
          <X className='h-4 w-4' />
        </Button>
      </div>

      <div className='space-y-4'>
        {/* 基本信息 */}
        <div>
          <h4 className='mb-2 text-sm font-medium'>{t('gallery.details.basicInfo')}</h4>
          <div className='space-y-2 text-xs'>
            <div className='flex justify-between'>
              <span className='text-muted-foreground'>{t('gallery.details.filename')}</span>
              <span className='max-w-32 truncate font-mono' title={activeAsset.filename}>
                {activeAsset.filename}
              </span>
            </div>
            <div className='flex justify-between'>
              <span className='text-muted-foreground'>{t('gallery.details.type')}</span>
              <Badge variant='secondary' className='px-2 text-xs'>
                {activeAsset.type}
              </Badge>
            </div>
            <div className='flex justify-between'>
              <span className='text-muted-foreground'>{t('gallery.details.mimeType')}</span>
              <span className='font-mono'>{activeAsset.mime_type}</span>
            </div>
          </div>
        </div>

        <Separator />

        {/* 尺寸信息 */}
        <div>
          <h4 className='mb-2 flex items-center gap-2 text-sm font-medium'>
            <Ruler className='h-3 w-3' />
            {t('gallery.details.sizeInfo')}
          </h4>
          <div className='space-y-2 text-xs'>
            {activeAsset.width && activeAsset.height && (
              <div className='flex justify-between'>
                <span className='text-muted-foreground'>{t('gallery.details.resolution')}</span>
                <span>
                  {activeAsset.width} × {activeAsset.height}
                </span>
              </div>
            )}
            {activeAsset.file_size && (
              <div className='flex justify-between'>
                <span className='text-muted-foreground'>{t('gallery.details.fileSize')}</span>
                <span>{formatBytes(activeAsset.file_size)}</span>
              </div>
            )}
          </div>
        </div>

        <Separator />

        {/* 时间信息 */}
        <div>
          <h4 className='mb-2 flex items-center gap-2 text-sm font-medium'>
            <Calendar className='h-3 w-3' />
            {t('gallery.details.timeInfo')}
          </h4>
          <div className='space-y-2 text-xs'>
            <div className='flex justify-between'>
              <span className='text-muted-foreground'>{t('gallery.details.createdAt')}</span>
              <span>{new Date(activeAsset.created_at).toLocaleString()}</span>
            </div>
            <div className='flex justify-between'>
              <span className='text-muted-foreground'>{t('gallery.details.updatedAt')}</span>
              <span>{new Date(activeAsset.updated_at).toLocaleString()}</span>
            </div>
          </div>
        </div>

        <Separator />

        {/* 文件路径 */}
        <div>
          <h4 className='mb-2 flex items-center gap-2 text-sm font-medium'>
            <HardDrive className='h-3 w-3' />
            {t('gallery.details.storagePath')}
          </h4>
          <div className='space-y-1 text-xs'>
            <div>
              <span className='text-muted-foreground'>{t('gallery.details.relativePath')}</span>
              <p className='mt-1 rounded bg-muted/50 p-2 font-mono text-xs break-all'>
                {activeAsset.relative_path}
              </p>
            </div>
          </div>
        </div>

        {/* 未来扩展：标签、评分等 */}
        <Separator />
        <div className='py-4 text-center text-xs text-muted-foreground'>
          {t('gallery.details.moreFeatures')}
        </div>
      </div>
    </div>
  )
}
