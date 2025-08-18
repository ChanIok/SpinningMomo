import { ActivityBar } from './ActivityBar'
import { ContentArea } from './ContentArea'
import { Header } from './Header'
import { Toaster } from '@/components/ui/sonner'
import { isWebView } from '@/lib/environment'

export default function AppLayout() {
  const showHeader = isWebView()
  
  return (
    <>
      <div
        className={`bg-background text-foreground flex h-screen w-screen flex-col`}
      >
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
  );
}
