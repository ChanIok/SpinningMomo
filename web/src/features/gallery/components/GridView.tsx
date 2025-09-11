import { useMemo, useEffect, useState } from 'react'
import { Grid } from 'react-window'
import type { CellComponentProps } from 'react-window'
import { AssetCard } from './AssetCard'
import { useAssetsStore } from '@/lib/assets/assetsStore'
import type { Asset } from '@/lib/assets/types'

interface GridViewProps {}

interface GridCellProps {
  assets: Asset[]
  columnCount: number
}

const GridCell = ({
  columnIndex,
  rowIndex,
  style,
  assets,
  columnCount,
}: CellComponentProps<GridCellProps>) => {
  const index = rowIndex * columnCount + columnIndex
  const asset = assets[index]

  if (!asset) {
    return <div style={style} />
  }

  return (
    <div style={{ ...style, padding: 8 }}>
      <AssetCard
        assetId={asset.id}
        viewMode='grid'
      />
    </div>
  )
}

export function GridView({}: GridViewProps = {}) {
  // 直接从 store 获取数据
  const assets = useAssetsStore((state) => state.assets)
  const viewConfig = useAssetsStore((state) => state.viewConfig)
  const [containerSize, setContainerSize] = useState({ width: 1200, height: 800 })

  // 根据 size 计算基础网格尺寸
  const cellSize = useMemo(() => {
    const sizeMultiplier = (viewConfig.size - 1) * 0.5 + 1 // 1-3倍大小
    const baseSize = 160
    return Math.floor(baseSize * sizeMultiplier) + 16 // +16px for padding
  }, [viewConfig.size])

  // 响应式列数计算
  const columnCount = useMemo(() => {
    const availableWidth = containerSize.width - 32 // 减去容器padding
    return Math.max(2, Math.floor(availableWidth / cellSize))
  }, [containerSize.width, cellSize])

  // 计算行数
  const rowCount = useMemo(() => {
    return Math.ceil(assets.length / columnCount)
  }, [assets.length, columnCount])

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

  // Grid cell props
  const cellProps = useMemo(
    () => ({
      assets,
      columnCount,
    }),
    [assets, columnCount]
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
      <Grid
        cellComponent={GridCell}
        columnCount={columnCount}
        columnWidth={cellSize}
        rowCount={rowCount}
        rowHeight={cellSize}
        cellProps={cellProps}
        style={{ height: containerSize.height, width: containerSize.width }}
      />
    </div>
  )
}
