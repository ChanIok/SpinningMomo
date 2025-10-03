import { useMemo, useCallback, useEffect, useState } from 'react'
import { MasonryPhotoAlbum } from 'react-photo-album'
import InfiniteScroll from 'react-photo-album/scroll'
import 'react-photo-album/masonry.css'
import { AssetCard } from './AssetCard'
import { useGalleryStore } from '@/lib/gallery/galleryStore'
import type { Asset } from '@/lib/gallery/types'
import type { Photo } from 'react-photo-album'

export function MasonryView() {
  // 直接从 store 获取数据
  const assets = useGalleryStore((state) => state.assets)
  const viewConfig = useGalleryStore((state) => state.viewConfig)
  const [containerHeight, setContainerHeight] = useState(800)

  // 转换 Asset 为 Photo 格式
  const photos = useMemo(() => {
    return assets
      .filter((asset) => asset.type === 'photo' && asset.width && asset.height)
      .map(
        (asset) =>
          ({
            src: `/api/assets/${asset.id}/thumbnail`, // 或你的缩略图URL
            width: asset.width!,
            height: asset.height!,
            key: asset.id.toString(),
            alt: asset.name,
            // 保存原始asset数据供后续使用
            asset,
          }) as Photo & { asset: Asset }
      )
  }, [assets])

  // 根据 size 计算列数
  const columns = useMemo(() => {
    const sizeMultiplier = (viewConfig.size - 1) * 0.5 + 1 // 调整列数范围
    return (containerWidth: number) => {
      const baseColumns = Math.floor(containerWidth / 250) // 基础列宽250px
      return Math.max(2, Math.floor(baseColumns / sizeMultiplier))
    }
  }, [viewConfig.size])

  // 更新容器高度
  useEffect(() => {
    const updateLayout = () => {
      setContainerHeight(window.innerHeight - 200) // 减去头部高度
    }

    updateLayout()
    window.addEventListener('resize', updateLayout)
    return () => window.removeEventListener('resize', updateLayout)
  }, [])

  // 自定义照片渲染函数
  const renderPhoto = useCallback((_props: any, context: any) => {
    const { photo, width, height } = context
    return (
      <div style={{ width, height }}>
        <AssetCard assetId={photo.asset.id} viewMode='masonry' />
      </div>
    )
  }, [])

  // 加载更多数据的函数
  const fetchMorePhotos = useCallback(async (index: number) => {
    // 每次加载50张图片
    try {
      // TODO: 替换为你的实际API调用
      // const response = await fetch(`/api/assets?offset=${index * 50}&limit=50`)
      // const newAssets = await response.json()

      // 暂时使用模拟数据，你可以根据实际情况修改
      console.log(
        `Loading batch ${index}, would load items ${index * 50} to ${(index + 1) * 50 - 1}`
      )

      // 如果已经加载了所有数据，返回null表示结束
      const loadedAssets = useGalleryStore.getState().assets
      if (loadedAssets.length >= (index + 1) * 50) {
        return null
      }

      // 返回新数据（这里需要根据你的实际API调整）
      return []
    } catch (error) {
      console.error('Failed to load more photos:', error)
      return null
    }
  }, [])

  if (photos.length === 0) {
    return (
      <div className='flex h-32 items-center justify-center text-muted-foreground'>
        <p>No items to display</p>
      </div>
    )
  }

  return (
    <div className='w-full' style={{ height: containerHeight }}>
      <InfiniteScroll
        photos={photos} // 初始数据
        fetch={fetchMorePhotos}
        fetchRootMargin='800px' // 提前800px开始加载
        offscreenRootMargin='2000px' // 超出2000px后卸载DOM
        singleton={false} // 更好的内存优化
        retries={3} // 失败重试3次
        loading={
          <div className='flex justify-center p-4'>
            <div className='text-muted-foreground'>加载中...</div>
          </div>
        }
        error={
          <div className='flex justify-center p-4'>
            <div className='text-destructive'>加载失败，请稍后重试</div>
          </div>
        }
        finished={
          <div className='flex justify-center p-4'>
            <div className='text-muted-foreground'>没有更多内容了</div>
          </div>
        }
      >
        <MasonryPhotoAlbum
          photos={[]} // InfiniteScroll会管理数据
          columns={columns}
          spacing={16}
          render={{
            photo: renderPhoto,
          }}
        />
      </InfiniteScroll>
    </div>
  )
}
