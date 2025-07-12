import { useEffect } from 'react'
import { SettingsSidebar } from './components/settings-sidebar'
import { SettingsContent } from './components/settings-content'
import { useSettingsStore } from './store/settings-store'

export function SettingsPage() {
  const { clearError } = useSettingsStore()

  // 清除之前的错误状态
  useEffect(() => {
    clearError()
  }, [clearError])

  return (
    <div className="h-full flex bg-background">
      {/* 设置侧边栏 */}
      <SettingsSidebar />
      
      {/* 设置内容区域 */}
      <SettingsContent />
    </div>
  )
}

export default SettingsPage 