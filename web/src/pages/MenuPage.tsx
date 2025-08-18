import { useEffect } from 'react'
import { MenuContent } from '@/features/menu/components/MenuContent'
import { useMenuStore } from '@/features/menu/store/menuStore'

export function MenuPage() {
  const { clearError } = useMenuStore()

  // 清除之前的错误状态
  useEffect(() => {
    clearError()
  }, [clearError])

  return (
    <div>
      {/* 菜单内容区域 */}
      <MenuContent />
    </div>
  )
}

export default MenuPage