import { Routes, Route } from 'react-router'
import { SettingsPage } from '@/features/settings'
import { MenuPage } from '@/features/menu'

export function ContentArea() {
  return (
    <div className='flex h-full flex-1 flex-col'>
      {/* 
        The header section that was previously here (with SidebarTrigger and Breadcrumb)
        is not included as its imports were removed in prior edits.
        This can be added back or re-designed later if needed.
      */}
      <main className='h-full flex-1 overflow-hidden'>
        <Routes>
          <Route path='/menu/*' element={<MenuPage />} />
          <Route path='/settings/*' element={<SettingsPage />} />
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
