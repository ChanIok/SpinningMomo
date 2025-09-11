import { ActivityBar } from './ActivityBar'
import { ContentArea } from './ContentArea'
import { Header } from './header'
import { Toaster } from '@/components/ui/sonner'
import { ThemeProvider } from '@/components/ThemeProvider'
import { SidebarProvider } from '@/components/ui/sidebar'
import { isWebView } from '@/lib/environment'
import { useLocation } from 'react-router'
import { useWebSettingsStore } from '@/lib/web-settings/webSettingsStore'

export default function AppLayout() {
  const showHeader = isWebView()
  const location = useLocation()
  const isHomePage = location.pathname === '/' || location.pathname.startsWith('/home')

  const { webSettings } = useWebSettingsStore()

  const backgroundSettings = webSettings.ui.background
  const shouldShowBackground = backgroundSettings.type === 'image' && backgroundSettings.imagePath

  // 背景图片URL - 只使用文件名，虚拟主机映射处理前缀
  const backgroundImageUrl = shouldShowBackground
    ? `/assets/${backgroundSettings.imagePath.split('/').pop()}?t=${new Date(webSettings.updatedAt).getTime()}`
    : ''

  return (
    <ThemeProvider>
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
            backdropFilter: `blur(${backgroundSettings.blurAmount}px)`,
            background: shouldShowBackground
              ? `color-mix(in srgb, var(--background) ${backgroundSettings.opacity * 100}%, transparent)`
              : 'var(--background-secondary)',
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
    </ThemeProvider>
  )
}
