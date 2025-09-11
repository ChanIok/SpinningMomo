import { useMemo, useEffect, useState } from 'react'
import { List } from 'react-window'
import type { RowComponentProps } from 'react-window'
import { AssetCard } from './AssetCard'
import { useAssetsStore } from '@/lib/assets/assetsStore'
import type { Asset } from '@/lib/assets/types'

interface ListViewProps {}

interface ListRowProps {
  assets: Asset[]
}

const ListRow = ({ index, style, assets }: RowComponentProps<ListRowProps>) => {
  const asset = assets[index]

  return (
    <div style={{ ...style, padding: '4px 16px' }}>
      <AssetCard
        assetId={asset.id}
        viewMode='list'
      />
    </div>
  )
}

export function ListView({}: ListViewProps = {}) {
  // 直接从 store 获取数据
  const assets = useAssetsStore((state) => state.assets)
  const viewConfig = useAssetsStore((state) => state.viewConfig)

  const [containerSize, setContainerSize] = useState({ width: 1200, height: 800 })

  // 根据 size 计算行高
  const itemHeight = useMemo(() => {
    const sizeMultiplier = (viewConfig.size - 1) * 0.2 + 1 // 1-1.8倍大小
    const baseHeight = 80
    return Math.floor(baseHeight * sizeMultiplier)
  }, [viewConfig.size])

  useEffect(() => {
    const updateSize = () => {
      setContainerSize({
        width: window.innerWidth,
        height: window.innerHeight - 200, // 减去头部高度
      })
    }

    updateSize()
    window.addEventListener('resize', updateSize)
    return () => window.removeEventListener('resize', updateSize)
  }, [])

  // List row props
  const rowProps = useMemo(
    () => ({
      assets,
    }),
    [assets]
  )

  if (assets.length === 0) {
    return (
      <div className='flex h-32 items-center justify-center text-muted-foreground'>
        <p>No items to display</p>
      </div>
    )
  }

  return (
    <div className='w-full' style={{ height: containerSize.height }}>
      <List
        rowComponent={ListRow}
        rowCount={assets.length}
        rowHeight={itemHeight}
        rowProps={rowProps}
        style={{ height: containerSize.height, width: containerSize.width }}
      />
    </div>
  )
}
