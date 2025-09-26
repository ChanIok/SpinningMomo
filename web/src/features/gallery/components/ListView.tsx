import { Virtuoso } from 'react-virtuoso'
import { AssetCard } from './AssetCard'
import { useGalleryStore } from '@/lib/gallery/galleryStore'

export function ListView() {
  // 直接从 store 获取数据
  const assets = useGalleryStore((state) => state.assets)

  // 过滤出有效的资产（与工作正常的视图保持一致）
  const validAssets = assets.filter((asset) => asset.type === 'photo' && asset.width && asset.height)

  if (validAssets.length === 0) {
    return (
      <div className='flex h-32 items-center justify-center text-muted-foreground'>
        <p>No items to display</p>
      </div>
    )
  }

  return (
    <div className='h-full w-full'>
      <Virtuoso
        totalCount={validAssets.length}
        itemContent={(index) => {
          const asset = validAssets[index]
          return (
            <div className='px-4 py-2'>
              <AssetCard
                assetId={asset.id}
                viewMode='list'
              />
            </div>
          )
        }}
      />
    </div>
  )
}
