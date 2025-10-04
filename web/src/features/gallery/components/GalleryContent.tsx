// import { useEffect } from 'react'
import { MasonryView } from './MasonryView'
import { GridView } from './GridView'
import { ListView } from './ListView'
import { AdaptiveView } from './AdaptiveView'
import { useGalleryStore } from '@/lib/gallery/galleryStore'
import { useAutoGalleryData } from '@/lib/gallery/hooks/useGalleryData'
import { useTranslation } from '@/lib/i18n'

export function GalleryContent() {
  // 使用真实数据
  const galleryData = useAutoGalleryData()
  const viewMode = useGalleryStore((state) => state.viewConfig.mode)
  const { t } = useTranslation()

  // 处理加载状态
  if (galleryData.isInitialLoading) {
    return (
      <div className='flex h-32 items-center justify-center text-muted-foreground'>
        <p>加载中...</p>
      </div>
    )
  }

  if (galleryData.error) {
    return (
      <div className='flex h-32 items-center justify-center text-destructive'>
        <p>{galleryData.error}</p>
      </div>
    )
  }

  if (galleryData.assets.length === 0) {
    return (
      <div className='flex h-32 items-center justify-center text-muted-foreground'>
        <p>{t('gallery.grid.noItems')}</p>
      </div>
    )
  }

  // 纯粹的组件切换器
  switch (viewMode) {
    case 'masonry':
      return <MasonryView />
    case 'grid':
      return <GridView />
    case 'list':
      return <ListView />
    case 'adaptive':
      return <AdaptiveView />
    default:
      return <MasonryView />
  }
}

// 导出所有视图组件供直接使用
export { MasonryView } from './MasonryView'
export { AdaptiveView } from './AdaptiveView'
export { GridView } from './GridView'
export { ListView } from './ListView'
