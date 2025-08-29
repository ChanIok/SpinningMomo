import { ActivityBar } from './ActivityBar'
import { ContentArea } from './ContentArea'
import { Header } from './Header'
import { Toaster } from '@/components/ui/sonner'
import { SidebarProvider } from '@/components/ui/sidebar'
import { isWebView } from '@/lib/environment'
import { useLocation } from 'react-router'
import { useWebSettingsStore } from '@/lib/web-settings/webSettingsStore'

export default function AppLayout() {
  const showHeader = isWebView()
  const location = useLocation()
  const isHomePage = location.pathname === '/' || location.pathname.startsWith('/home')

  const { settings } = useWebSettingsStore()

  const backgroundSettings = settings.ui.background
  const shouldShowBackground = backgroundSettings.type === 'image' && backgroundSettings.imagePath

  // 背景图片URL - 只使用文件名，虚拟主机映射处理前缀
  const backgroundImageUrl = shouldShowBackground
    ? `/assets/${backgroundSettings.imagePath.split('/').pop()}?t=${new Date(settings.updatedAt).getTime()}`
    : ''

  return (
    <>
      {/* Background Layer */}
      <div
        className='fixed inset-0 z-[-2] bg-cover bg-center bg-no-repeat'
        style={{
          backgroundImage: backgroundImageUrl ? `url('${backgroundImageUrl}')` : "url('')",
        }}
      />

      {/* Glass Overlay Layer */}
      <div
        className={`fixed inset-0 z-[-1] transition-opacity duration-300 ${
          isHomePage ? 'opacity-0' : 'opacity-100'
        }`}
        style={{
          backdropFilter: 'blur(60px)',
          background: `rgba(255, 255, 255, ${shouldShowBackground ? backgroundSettings.opacity : 1})`,
        }}
      />

      <SidebarProvider>
        <div className='relative flex h-screen w-screen flex-col text-foreground'>
          {showHeader && <Header />}

          <div className='flex flex-1 overflow-hidden'>
            <ActivityBar />

            {/* Main Layout Area (Content) */}
            <div className='flex flex-1 overflow-hidden'>
              <ContentArea />
            </div>
          </div>
        </div>
      </SidebarProvider>
      <Toaster />
    </>
  )
}
