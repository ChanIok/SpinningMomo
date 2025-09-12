import { ScrollArea } from '@/components/ui/scroll-area'
import 'allotment/dist/style.css'
import { useAssetsStore } from '@/lib/assets/assetsStore'
import { useAssets } from '@/lib/assets/hooks/useAssets'
import { useGalleryKeyboard, useGalleryView, useGalleryLayout } from '../hooks'
import { GallerySidebar } from './GallerySidebar'
import { GalleryViewer } from './GalleryViewer'
import { GalleryDetails } from './GalleryDetails'
import { GalleryLightbox } from '../components/GalleryLightbox'
import { PanelGroup, Panel, PanelResizer } from '@window-splitter/react'

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
      <div className='flex h-full w-full'>
        <PanelGroup className='flex h-full w-full' autosaveId='autosave'>
          <Panel id='sidebar-panel' collapsible min='100px' max='400px' default='280px' isStaticAtRest >
            {isSidebarOpen && <GallerySidebar />}
          </Panel>
          <PanelResizer size='4px' id='sidebar-resizer' />

          <Panel min='400px' id='viewer-panel'>
            <GalleryViewer />
          </Panel>

          <PanelResizer size='4px' id='details-resizer' />
          <Panel id='details-panel' collapsible min='100px' max='400px' default='280px' isStaticAtRest>
            {isDetailsOpen && (
              <ScrollArea className='h-full'>
                <div className='border-l'>
                  <GalleryDetails />
                </div>
              </ScrollArea>
            )}
          </Panel>
        </PanelGroup>
      </div>

      {lightbox.isOpen && <GalleryLightbox />}
    </>
  )
}
