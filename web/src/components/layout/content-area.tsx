import { Routes, Route } from 'react-router'
import { MenuPage } from '@/features/menu'
import { LayoutPage } from '@/features/layout'
import { SettingsPage } from '@/features/settings'
import { FunctionPage } from '@/features/function'
import { AboutPage } from '@/features/about'

export function ContentArea() {
  return (
    <div className='flex h-full flex-1 flex-col'>
      {/* 
        The header section that was previously here (with SidebarTrigger and Breadcrumb)
        is not included as its imports were removed in prior edits.
        This can be added back or re-designed later if needed.
      */}
      <main className='h-full flex-1 overflow-auto'>
        <Routes>
          <Route path='/menu/*' element={<MenuPage />} />
          <Route path='/layout/*' element={<LayoutPage />} />
          <Route path='/function/*' element={<FunctionPage />} />
          <Route path='/settings/*' element={<SettingsPage />} />
          <Route path='/about/*' element={<AboutPage />} />
          <Route
            path='*'
            element={
              <div className='text-xl font-semibold pt-10 text-center'>欢迎使用 SpinningMomo</div>
            }
          />
        </Routes>
      </main>
    </div>
  )
}
