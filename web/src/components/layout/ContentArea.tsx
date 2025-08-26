import { Routes, Route } from 'react-router'
import { SettingsPage, AboutPage, HomePage } from '@/pages'

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
          <Route path='/home/*' element={<HomePage />} />
          <Route path='/settings/*' element={<SettingsPage />} />
          <Route path='/about/*' element={<AboutPage />} />
          <Route path='*' element={<HomePage />} />
        </Routes>
      </main>
    </div>
  )
}
