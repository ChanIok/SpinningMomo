import { forwardRef } from 'react'
import { VirtuosoGrid } from 'react-virtuoso'
import { AssetCard } from './AssetCard'
import { useGalleryStore } from '@/lib/gallery/galleryStore'

// 定义 gridComponents，确保它在组件外部以避免重新挂载
const gridComponents = {
  // List 容器使用 Flexbox wrap 来实现自动换行
  List: forwardRef<HTMLDivElement, React.HTMLProps<HTMLDivElement>>(
    ({ style, children, ...props }, ref) => (
      <div
        ref={ref}
        {...props}
        style={{
          display: 'flex',
          flexWrap: 'wrap',
          padding: '16px', // 统一的内边距
          ...style,
        }}
      >
        {children}
      </div>
    )
  ),
  // Item 容器定义每个单元格的尺寸和间距
  Item: ({ children, ...props }: { children?: React.ReactNode }) => (
    <div
      {...props}
      style={{
        padding: '8px', // 单元格之间的间距，通过padding实现
        flex: '1 0 250px', // 响应式的核心：基础宽度250px，可放大
        minWidth: '250px', // 确保不会被过度压缩
        boxSizing: 'border-box',
      }}
    >
      {children}
    </div>
  ),
}

export function GridView() {
  // 直接从 store 获取数据
  const assets = useGalleryStore((state) => state.assets)

  // 过滤出有效的资产
  const validAssets = assets.filter(
    (asset) => asset.type === 'photo' && asset.width && asset.height
  )

  if (validAssets.length === 0) {
    return (
      <div className='flex h-32 items-center justify-center text-muted-foreground'>
        <p>No items to display</p>
      </div>
    )
  }

  return (
    <div className='h-full w-full'>
      <VirtuosoGrid
        totalCount={validAssets.length}
        components={gridComponents} // 使用定义好的静态组件
        itemContent={(index) => {
          const asset = validAssets[index]
          return <AssetCard assetId={asset.id} viewMode='grid' />
        }}
      />
    </div>
  )
}
