import { useEffect } from 'react'
import { AppearanceContent } from '@/features/appearance/components/AppearanceContent'
import { useAppearanceStore } from '@/features/appearance/store/appearanceStore'

export function AppearancePage() {
  const { clearError } = useAppearanceStore()

  // 清除之前的错误状态
  useEffect(() => {
    clearError()
  }, [clearError])

  return (
    <div>
      {/* 外观内容区域 */}
      <AppearanceContent />
    </div>
  )
}

export default AppearancePage
