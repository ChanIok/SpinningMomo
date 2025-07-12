import { useSettingsStore } from '../store/settings-store'
import { AboutPage } from '../pages/about'
import { GeneralPage } from '../pages/general'
import { AdvancedPage } from '../pages/advanced'

export function SettingsContent() {
  const { currentPage } = useSettingsStore()

  const renderCurrentPage = () => {
    switch (currentPage) {
      case 'about':
        return <AboutPage />
      case 'general':
        return <GeneralPage />
      case 'advanced':
        return <AdvancedPage />
      default:
        return <GeneralPage />
    }
  }

  return (
    <div className="flex-1 overflow-hidden">
      <div className="h-full overflow-y-auto">
        {renderCurrentPage()}
      </div>
    </div>
  )
} 