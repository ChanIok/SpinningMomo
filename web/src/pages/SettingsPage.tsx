import { useEffect } from 'react'
import { SettingsContent } from '@/features/settings/components/SettingsContent'
import { useSettingsStore } from '@/lib/settings'

export function SettingsPage() {
  const { clearError } = useSettingsStore()

  // 清除之前的错误状态
  useEffect(() => {
    clearError()
  }, [clearError])

  return (
    <div>
      {/* 设置内容区域 */}
      <SettingsContent />
    </div>
  )
}

export default SettingsPage
