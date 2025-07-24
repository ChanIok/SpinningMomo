import { useEffect } from 'react'
import { LayoutContent } from '../components/layout-content'
import { useLayoutStore } from '../store/layout-store'

export function LayoutPage() {
  const { clearError } = useLayoutStore()

  // 清除之前的错误状态
  useEffect(() => {
    clearError()
  }, [clearError])

  return (
    <div className="h-full flex bg-background">
      {/* 布局内容区域 */}
      <LayoutContent />
    </div>
  )
}

export default LayoutPage