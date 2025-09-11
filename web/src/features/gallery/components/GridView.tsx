import { useMemo } from 'react'
import { Grid } from 'react-window'
import type { CellComponentProps } from 'react-window'
import { AssetCard } from './AssetCard'
import { useAssetsStore } from '@/lib/assets/assetsStore'
import { useGalleryView } from '../hooks'
import type { Asset } from '@/lib/assets/types'

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
      <AssetCard assetId={asset.id} viewMode='grid' />
    </div>
  )
}

export function GridView() {
  // 从 store 获取数据
  const assets = useAssetsStore((state) => state.assets)

  // 使用 gallery view hook
  const view = useGalleryView({ headerHeight: 200 })

  // 获取网格配置
  const { columnCount, columnWidth, rowHeight, containerWidth, containerHeight } =
    view.getGridConfig

  // 计算行数
  const rowCount = view.getRowCount(assets.length)

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
    <div className='w-full' style={{ height: containerHeight }}>
      <Grid
        cellComponent={GridCell}
        columnCount={columnCount}
        columnWidth={columnWidth}
        rowCount={rowCount}
        rowHeight={rowHeight}
        cellProps={cellProps}
        style={{ height: containerHeight, width: containerWidth }}
      />
    </div>
  )
}
