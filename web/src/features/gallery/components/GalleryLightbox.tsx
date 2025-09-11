import { useEffect, useCallback } from 'react'
import { Button } from '@/components/ui/button'
import { X, ChevronLeft, ChevronRight } from 'lucide-react'
import { useAssetsStore } from '@/lib/assets/assetsStore'
import { getMockAssetUrl } from '@/lib/assets/mockData'
import { cn } from '@/lib/utils'

export function GalleryLightbox() {
  const { lightbox, closeLightbox, goToPreviousLightbox, goToNextLightbox, goToLightboxIndex } =
    useAssetsStore()

  const currentAsset = lightbox.assets[lightbox.currentIndex]

  // 键盘快捷键
  const handleKeyDown = useCallback(
    (e: KeyboardEvent) => {
      switch (e.key) {
        case 'Escape':
          closeLightbox()
          break
        case 'ArrowLeft':
          goToPreviousLightbox()
          break
        case 'ArrowRight':
          goToNextLightbox()
          break
      }
    },
    [closeLightbox, goToPreviousLightbox, goToNextLightbox]
  )

  useEffect(() => {
    window.addEventListener('keydown', handleKeyDown)
    return () => window.removeEventListener('keydown', handleKeyDown)
  }, [handleKeyDown])

  // 阻止背景滚动
  useEffect(() => {
    document.body.style.overflow = 'hidden'
    return () => {
      document.body.style.overflow = 'unset'
    }
  }, [])

  if (!currentAsset) {
    return null
  }

  const canGoPrevious = lightbox.currentIndex > 0
  const canGoNext = lightbox.currentIndex < lightbox.assets.length - 1

  return (
    <div className='fixed inset-0 z-50 bg-black/90 backdrop-blur-sm'>
      {/* 顶部工具栏 */}
      <div className='absolute top-0 right-0 left-0 z-10 bg-gradient-to-b from-black/50 to-transparent p-4'>
        <div className='flex items-center justify-between'>
          <div className='text-white'>
            <h2 className='max-w-md truncate text-lg font-medium'>{currentAsset.filename}</h2>
            <p className='text-sm text-white/70'>
              {lightbox.currentIndex + 1} / {lightbox.assets.length}
              {currentAsset.width && currentAsset.height && (
                <span className='ml-2'>
                  • {currentAsset.width} × {currentAsset.height}
                </span>
              )}
            </p>
          </div>
          <Button
            variant='ghost'
            size='icon'
            className='text-white hover:bg-white/20'
            onClick={closeLightbox}
          >
            <X className='h-6 w-6' />
          </Button>
        </div>
      </div>

      {/* 主图片区域 */}
      <div
        className='flex h-full items-center justify-center px-4 py-20'
        onClick={(e) => {
          // 点击空白区域关闭
          if (e.target === e.currentTarget) {
            closeLightbox()
          }
        }}
      >
        <div className='relative max-h-full max-w-full'>
          <img
            src={getMockAssetUrl(currentAsset.id)}
            alt={currentAsset.filename}
            className='max-h-full max-w-full object-contain'
            draggable={false}
          />
        </div>
      </div>

      {/* 左右导航按钮 */}
      {canGoPrevious && (
        <Button
          variant='ghost'
          size='icon'
          className='absolute top-1/2 left-4 h-12 w-12 -translate-y-1/2 text-white hover:bg-white/20'
          onClick={goToPreviousLightbox}
        >
          <ChevronLeft className='h-8 w-8' />
        </Button>
      )}

      {canGoNext && (
        <Button
          variant='ghost'
          size='icon'
          className='absolute top-1/2 right-4 h-12 w-12 -translate-y-1/2 text-white hover:bg-white/20'
          onClick={goToNextLightbox}
        >
          <ChevronRight className='h-8 w-8' />
        </Button>
      )}

      {/* 底部缩略图导航 */}
      <div className='absolute right-0 bottom-0 left-0 bg-gradient-to-t from-black/50 to-transparent p-4'>
        <div className='flex items-center justify-center gap-2 overflow-x-auto pb-2'>
          {lightbox.assets.map((asset, index) => (
            <button
              key={asset.id}
              className={cn(
                'h-16 w-16 flex-shrink-0 overflow-hidden rounded-lg border-2 transition-all',
                index === lightbox.currentIndex
                  ? 'scale-110 border-white'
                  : 'border-transparent opacity-70 hover:border-white/50 hover:opacity-100'
              )}
              onClick={() => goToLightboxIndex(index)}
            >
              <img
                src={getMockAssetUrl(asset.id)}
                alt={asset.filename}
                className='h-full w-full object-cover'
              />
            </button>
          ))}
        </div>
      </div>
    </div>
  )
}
