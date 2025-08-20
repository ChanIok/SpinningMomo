import { ActivityBar } from './ActivityBar'
import { ContentArea } from './ContentArea'
import { Header } from './Header'
import { Toaster } from '@/components/ui/sonner'
import { isWebView } from '@/lib/environment'
import { useLocation } from 'react-router'

export default function AppLayout() {
  const showHeader = isWebView()
  const location = useLocation()
  const isHomePage = location.pathname === '/' || location.pathname.startsWith('/home')

  return (
    <>
      {/* Background Layer */}
      <div
        className='fixed inset-0 z-[-2] bg-cover bg-center bg-no-repeat'
        style={{
          backgroundImage:
            "url('http://localhost:8080/2025_03_30_02_33_09_3140045.jpeg')",
        }}
      />

      {/* Glass Overlay Layer */}
      <div
        className={`fixed inset-0 z-[-1] transition-opacity duration-300 ${
          isHomePage ? 'opacity-0' : 'opacity-100'
        }`}
        style={{
          backdropFilter: 'blur(60px)',
          background: 'rgba(255, 255, 255, 0.9)',
        }}
      />

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
      <Toaster />
    </>
  )
}
