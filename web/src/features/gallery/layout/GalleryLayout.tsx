import { ScrollArea } from '@/components/ui/scroll-area'
import { ResizablePanelGroup, ResizablePanel, ResizableHandle } from '@/components/ui/resizable'
import { useAssetsStore } from '@/lib/assets/assetsStore'
import { useAssets } from '@/lib/assets/hooks/useAssets'
import { GallerySidebar } from './GallerySidebar'
import { GalleryViewer } from './GalleryViewer'
import { GalleryDetails } from './GalleryDetails'
import { GalleryLightbox } from '../components/GalleryLightbox'

export function GalleryLayout() {
  const { sidebar, detailsOpen, lightbox } = useAssetsStore()
  // Layout state is controlled by the store directly

  // 初始化资产数据
  useAssets({ useMockData: true, autoLoad: true })

  return (
    <>
      {/* 主布局 */}
      <div className='flex h-full w-full'>
        <ResizablePanelGroup direction='horizontal' className='h-full w-full'>
          {/* 左侧边栏 */}
          {sidebar.isOpen && (
            <>
              <ResizablePanel defaultSize={25} minSize={15} maxSize={40}>
                <GallerySidebar />
              </ResizablePanel>
              <ResizableHandle />
            </>
          )}

          {/* 主内容区域 */}
          <ResizablePanel defaultSize={detailsOpen ? 50 : 75} minSize={30}>
            <GalleryViewer />
          </ResizablePanel>

          {/* 右侧详情面板 */}
          {detailsOpen && (
            <>
              <ResizableHandle />
              <ResizablePanel defaultSize={25} minSize={15} maxSize={40}>
                <ScrollArea className='h-full'>
                  <div className='border-l'>
                    <GalleryDetails />
                  </div>
                </ScrollArea>
              </ResizablePanel>
            </>
          )}
        </ResizablePanelGroup>
      </div>

      {/* Lightbox 弹层 */}
      {lightbox.isOpen && <GalleryLightbox />}
    </>
  )
}
