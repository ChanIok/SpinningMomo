import { ScrollArea } from '@/components/ui/scroll-area'
import { Skeleton } from '@/components/ui/skeleton'
import { useAssetsStore } from '@/lib/assets/assetsStore'
import { GalleryToolbar } from '../components/GalleryToolbar'
import { GalleryContent } from '../components/GalleryContent'
import { useTranslation } from '@/lib/i18n'

export function GalleryViewer() {
  // 只获取布局需要的状态
  const { assets, isLoading, isInitialLoading, error } = useAssetsStore()
  const { t } = useTranslation()

  if (isInitialLoading) {
    return (
      <div className='flex h-full flex-col'>
        <div className='border-b p-4'>
          <Skeleton className='h-10 w-full' />
        </div>
        <div className='flex-1 p-4'>
          <GalleryContentSkeleton />
        </div>
      </div>
    )
  }

  if (error) {
    return (
      <div className='flex h-full flex-col'>
        <GalleryToolbar />
        <div className='flex flex-1 items-center justify-center'>
          <div className='text-center text-muted-foreground'>
            <p className='mb-2 text-lg'>{t('gallery.viewer.loadingFailed')}</p>
            <p className='text-sm'>{error}</p>
          </div>
        </div>
      </div>
    )
  }

  if (!isLoading && assets.length === 0) {
    return (
      <div className='flex h-full flex-col'>
        <GalleryToolbar />
        <div className='flex flex-1 items-center justify-center'>
          <div className='text-center text-muted-foreground'>
            <p className='mb-2 text-lg'>{t('gallery.viewer.noItems')}</p>
            <p className='text-sm'>{t('gallery.viewer.noItemsDescription')}</p>
          </div>
        </div>
      </div>
    )
  }

  return (
    <div className='flex h-full flex-col'>
      {/* 工具栏 */}
      <GalleryToolbar />

      {/* 资产网格 - Virtuoso 自带滚动，不再需要 ScrollArea */}
      <div className='flex-1'>
        <GalleryContent />

        {isLoading && (
          <div className='mt-4 p-4'>
            <GalleryContentSkeleton />
          </div>
        )}
      </div>
    </div>
  )
}

// 骨架屏组件
function GalleryContentSkeleton() {
  return (
    <div className='grid grid-cols-2 gap-4 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5 xl:grid-cols-6'>
      {Array.from({ length: 24 }).map((_, i) => (
        <div key={i} className='space-y-2'>
          <Skeleton className='aspect-square w-full rounded-lg' />
          <Skeleton className='h-3 w-3/4' />
        </div>
      ))}
    </div>
  )
}
