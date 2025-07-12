import { ActivityBar } from './activity-bar'
import { ContentArea } from './content-area'
import { Header } from './header'
import { Toaster } from '@/components/ui/sonner'

export default function AppLayout() {
  return (
    <>
      <div
        className={`bg-background text-foreground flex h-screen w-screen flex-col`}
      >
        <Header />

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
