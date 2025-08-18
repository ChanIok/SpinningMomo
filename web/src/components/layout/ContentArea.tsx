import { Routes, Route } from 'react-router'
import { MenuPage, AppearancePage, SettingsPage, FunctionPage, AboutPage } from '@/pages'

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
          <Route path='/appearance/*' element={<AppearancePage />} />
          <Route path='/function/*' element={<FunctionPage />} />
          <Route path='/settings/*' element={<SettingsPage />} />
          <Route path='/about/*' element={<AboutPage />} />
          <Route
            path='*'
            element={
              <div className='pt-10 text-center text-xl font-semibold'>欢迎使用 SpinningMomo</div>
            }
          />
        </Routes>
      </main>
    </div>
  )
}
