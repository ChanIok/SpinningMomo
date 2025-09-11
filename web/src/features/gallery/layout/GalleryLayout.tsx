import { ScrollArea } from '@/components/ui/scroll-area'
import { Allotment } from 'allotment'
import 'allotment/dist/style.css'
import { useAssetsStore } from '@/lib/assets/assetsStore'
import { useAssets } from '@/lib/assets/hooks/useAssets'
import { useGalleryKeyboard, useGalleryView, useGalleryLayout } from '../hooks'
import { GallerySidebar } from './GallerySidebar'
import { GalleryViewer } from './GalleryViewer'
import { GalleryDetails } from './GalleryDetails'
import { GalleryLightbox } from '../components/GalleryLightbox'

export function GalleryLayout() {
  const { isSidebarOpen, isDetailsOpen } = useGalleryLayout()

  const { lightbox } = useAssetsStore()

  const view = useGalleryView()

  useGalleryKeyboard({
    enableGlobalShortcuts: true,
    columnsPerRow: view.columnCount,
  })

  useAssets({ useMockData: true, autoLoad: true })

  return (
    <>
      {/* Main Layout */}
      <div className='flex h-full w-full'>
        <Allotment>
          {/* Left Sidebar */}
          {isSidebarOpen && (
            <Allotment.Pane preferredSize={250} minSize={150} maxSize={400} snap>
              <GallerySidebar />
            </Allotment.Pane>
          )}

          {/* Main Content Area */}
          <Allotment.Pane minSize={500}>
            <GalleryViewer />
          </Allotment.Pane>

          {/* Right Details Panel */}
          {isDetailsOpen && (
            <Allotment.Pane preferredSize={300} minSize={200} maxSize={500} snap>
              <ScrollArea className='h-full'>
                <div className='border-l'>
                  <GalleryDetails />
                </div>
              </ScrollArea>
            </Allotment.Pane>
          )}
        </Allotment>
      </div>

      {/* Lightbox Overlay */}
      {lightbox.isOpen && <GalleryLightbox />}
    </>
  )
}
