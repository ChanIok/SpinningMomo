import { useState, useMemo, useCallback, type MouseEvent } from 'react'
import { Checkbox } from '@/components/ui/checkbox'
import { Badge } from '@/components/ui/badge'
import { FileImage, Image as ImageIcon, Video, Zap } from 'lucide-react'
import type { ViewMode } from '@/lib/assets/types'
import { useAssetsStore } from '@/lib/assets/assetsStore'
import { useGallerySelection, useGalleryLightbox } from '../hooks'
import { getMockThumbnailUrl } from '@/lib/assets/mockData'
import { cn, formatBytes } from '@/lib/utils'

interface AssetCardProps {
  assetId: number
  viewMode: ViewMode
}

export function AssetCard({ assetId, viewMode }: AssetCardProps) {
  // 从 store 获取资产数据
  const asset = useAssetsStore((state) => state.assets.find((a) => a.id === assetId))

  // 使用 gallery hooks
  const selection = useGallerySelection()
  const lightbox = useGalleryLightbox()

  // 计算选择和活跃状态
  const isSelected = selection.selection.selectedIds.has(assetId)
  const isActive = selection.selection.activeId === assetId

  const [imageError, setImageError] = useState(false)
  const [imageLoaded, setImageLoaded] = useState(false)

  // 事件处理函数
  const handleClick = useCallback(() => {
    if (asset) {
      selection.handleAssetClick(asset)
    }
  }, [asset, selection])

  const handleDoubleClick = useCallback(() => {
    if (asset) {
      lightbox.openLightboxWithAsset(asset)
    }
  }, [asset, lightbox])

  const handleSelect = useCallback(
    (selected: boolean) => {
      selection.handleAssetSelect(assetId, selected, true)
    },
    [assetId, selection]
  )

  // 计算真实长宽比
  const aspectRatio = useMemo(() => {
    if (
      !asset ||
      (viewMode !== 'masonry' && viewMode !== 'adaptive') ||
      !asset.width ||
      !asset.height
    ) {
      return undefined
    }
    return asset.height / asset.width
  }, [asset?.width, asset?.height, viewMode])

  // 如果找不到资产，不渲染
  if (!asset) {
    return null
  }

  // 获取资产类型图标
  const getTypeIcon = () => {
    switch (asset.type) {
      case 'photo':
        return ImageIcon
      case 'video':
        return Video
      case 'live_photo':
        return Zap
      default:
        return FileImage
    }
  }

  const TypeIcon = getTypeIcon()

  // 处理图片加载错误
  const handleImageError = () => {
    setImageError(true)
  }

  // 处理图片加载完成
  const handleImageLoad = () => {
    setImageLoaded(true)
  }

  // 处理选择状态变化
  const handleCheckboxChange = (e: MouseEvent) => {
    e.stopPropagation()
    handleSelect(!isSelected)
  }

  // 列表模式的布局
  if (viewMode === 'list') {
    return (
      <div
        className={cn(
          'group flex cursor-pointer items-center gap-3 rounded-lg border p-3 transition-all hover:bg-accent/50',
          isActive && 'ring-2 ring-primary',
          isSelected && 'bg-accent/30'
        )}
        onClick={handleClick}
        onDoubleClick={handleDoubleClick}
      >
        {/* 选择框 */}
        <div
          className='opacity-0 transition-opacity group-hover:opacity-100'
          onClick={handleCheckboxChange}
        >
          <Checkbox checked={isSelected} />
        </div>

        {/* 缩略图 */}
        <div className='relative h-12 w-12 flex-shrink-0'>
          {!imageError && !imageLoaded && (
            <div className='absolute inset-0 animate-pulse rounded bg-muted' />
          )}
          {!imageError ? (
            <img
              src={getMockThumbnailUrl(assetId)}
              alt={asset.filename}
              className={cn(
                'h-full w-full rounded object-cover transition-opacity',
                imageLoaded ? 'opacity-100' : 'opacity-0'
              )}
              onLoad={handleImageLoad}
              onError={handleImageError}
            />
          ) : (
            <div className='flex h-full w-full items-center justify-center rounded bg-muted'>
              <TypeIcon className='h-6 w-6 text-muted-foreground' />
            </div>
          )}
        </div>

        {/* 文件信息 */}
        <div className='min-w-0 flex-1'>
          <div className='mb-1 flex items-center gap-2'>
            <p className='truncate text-sm font-medium'>{asset.filename}</p>
            <Badge variant='secondary' className='px-1.5 text-xs'>
              {asset.type}
            </Badge>
          </div>
          <div className='flex items-center gap-4 text-xs text-muted-foreground'>
            {asset.width && asset.height && (
              <span>
                {asset.width} × {asset.height}
              </span>
            )}
            {asset.file_size && <span>{formatBytes(asset.file_size)}</span>}
            <span>{new Date(asset.created_at).toLocaleDateString()}</span>
          </div>
        </div>
      </div>
    )
  }

  // 网格和瀑布流模式的布局
  return (
    <div
      className={cn(
        'group relative cursor-pointer transition-all hover:scale-[1.02]',
        isActive && 'ring-2 ring-primary ring-offset-2',
        viewMode === 'masonry' || viewMode === 'adaptive' ? 'w-full' : 'aspect-square'
      )}
      onClick={handleClick}
      onDoubleClick={handleDoubleClick}
    >
      {/* 选择框 */}
      <div
        className={cn(
          'absolute top-2 left-2 z-10 opacity-0 transition-opacity group-hover:opacity-100',
          isSelected && 'opacity-100'
        )}
        onClick={handleCheckboxChange}
      >
        <Checkbox checked={isSelected} className='bg-background/80 backdrop-blur-sm' />
      </div>

      {/* 类型图标 */}
      <div className='absolute top-2 right-2 z-10'>
        <div className='rounded-md bg-background/80 p-1 backdrop-blur-sm'>
          <TypeIcon className='h-4 w-4 text-muted-foreground' />
        </div>
      </div>

      {/* 主图片 */}
      <div
        className={cn(
          'relative overflow-hidden rounded-lg bg-muted',
          viewMode === 'masonry' || viewMode === 'adaptive' ? 'w-full' : 'h-full w-full'
        )}
        style={
          (viewMode === 'masonry' || viewMode === 'adaptive') && aspectRatio
            ? { aspectRatio: `1 / ${aspectRatio}` }
            : undefined
        }
      >
        {!imageError && !imageLoaded && (
          <div
            className={cn(
              'absolute inset-0 animate-pulse bg-muted',
              viewMode === 'masonry' || viewMode === 'adaptive' ? 'w-full' : 'h-full w-full'
            )}
            style={
              (viewMode === 'masonry' || viewMode === 'adaptive') && aspectRatio
                ? { aspectRatio: `1 / ${aspectRatio}` }
                : undefined
            }
          />
        )}

        {!imageError ? (
          <img
            src={getMockThumbnailUrl(assetId)}
            alt={asset.filename}
            className={cn(
              'object-cover transition-all duration-300 group-hover:scale-110',
              imageLoaded ? 'opacity-100' : 'opacity-0',
              'h-full w-full'
            )}
            onLoad={handleImageLoad}
            onError={handleImageError}
            loading='lazy'
          />
        ) : (
          <div className={cn('flex items-center justify-center bg-muted', 'h-full w-full')}>
            <TypeIcon className='h-8 w-8 text-muted-foreground' />
          </div>
        )}

        {/* 悬浮信息覆层 */}
        <div className='absolute inset-0 flex items-end bg-black/60 opacity-0 transition-opacity group-hover:opacity-100'>
          <div className='w-full p-3 text-white'>
            <p className='mb-1 truncate text-sm font-medium'>{asset.filename}</p>
            <div className='flex items-center justify-between text-xs'>
              <div>
                {asset.width && asset.height && (
                  <span>
                    {asset.width} × {asset.height}
                  </span>
                )}
              </div>
              <div>{asset.file_size && formatBytes(asset.file_size)}</div>
            </div>
          </div>
        </div>
      </div>

      {/* 底部标题（仅在网格模式且不悬浮时显示） */}
      {viewMode === 'grid' && (
        <div className='mt-2 px-1'>
          <p className='truncate text-xs text-muted-foreground'>{asset.filename}</p>
        </div>
      )}
    </div>
  )
}
