import { MasonryView } from './MasonryView'
import { GridView } from './GridView'
import { ListView } from './ListView'
import { AdaptiveView } from './AdaptiveView'
import { useAssetsStore } from '@/lib/assets/assetsStore'
import { useTranslation } from '@/lib/i18n'

export function GalleryContent() {
  // 直接从 store 获取数据
  const assets = useAssetsStore((state) => state.assets)
  const viewMode = useAssetsStore((state) => state.viewConfig.mode)
  const { t } = useTranslation()

  if (assets.length === 0) {
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
