import { cn } from '@/lib/utils'
import { useState, useEffect } from 'react'
import { useNavigate, useLocation } from 'react-router'
import { Menu, Settings, Palette, Wrench, Info } from 'lucide-react'

interface MenuItem {
  key?: string
  label?: string
  icon?: React.ElementType
  type?: 'divider'
}

const menuItems: MenuItem[] = [
  { label: '菜单', key: 'menu', icon: Menu },
  { label: '布局', key: 'layout', icon: Palette },
  { label: '功能', key: 'function', icon: Wrench },
  { type: 'divider' },
  { label: '设置', key: 'settings', icon: Settings },
  { label: '关于', key: 'about', icon: Info },
]

export function ActivityBar() {
  const navigate = useNavigate()
  const location = useLocation()
  const [activeKey, setActiveKey] = useState<string | null>(null)

  useEffect(() => {
    const currentPath = location.pathname || ''
    const pathSegments = currentPath.split('/')
    const pathSegment = pathSegments.length > 1 ? pathSegments[1] : null

    if (pathSegment) {
      setActiveKey(pathSegment)
    } else {
      // No specific segment, e.g., root path. Clear active key or set a default.
      setActiveKey(null)
    }
  }, [location.pathname])

  const handleMenuSelect = (key: string) => {
    navigate(`/${key}`)
  }

  return (
    <div
      className={cn(
        'bg-background border-border flex h-full flex-col border-r',
        // 桌面端：固定宽度
        'w-16 md:w-16',
        // 移动端：底部导航栏样式 (可选，现在先保持侧边栏)
        'sm:w-16'
      )}
    >
      <div className='p-2 flex flex-1 flex-col'>
        <nav className='space-y-1 flex flex-col'>
          {menuItems.map((item, index) =>
            item.type === 'divider' ? (
              <div key={`divider-${index}`} className='mx-2 my-3 border-border border-t' />
            ) : (
              <div key={item.key} className='group relative'>
                <button
                  onClick={() => item.key && handleMenuSelect(item.key)}
                  title={item.label}
                  className={cn(
                    'h-12 w-12 rounded-lg relative flex cursor-pointer items-center justify-center transition-all duration-200',
                    'hover:bg-accent hover:text-accent-foreground',
                    'focus-visible:ring-ring focus-visible:ring-2 focus-visible:ring-offset-2 focus-visible:outline-none',
                    {
                      'bg-primary text-primary-foreground shadow-sm': activeKey === item.key,
                      'text-muted-foreground': activeKey !== item.key,
                    }
                  )}
                >
                  {item.icon && (
                    <item.icon className='h-5 w-5' strokeWidth={activeKey === item.key ? 2.5 : 2} />
                  )}
                </button>

                {/* 工具提示 - 桌面端显示 */}
                {item.label && item.key && (
                  <div
                    className={cn(
                      'ml-3 px-3 py-2 bg-popover text-popover-foreground text-sm rounded-md shadow-md border-border absolute left-full border',
                      'invisible z-50 whitespace-nowrap opacity-0 transition-all duration-200 group-hover:visible group-hover:opacity-100',
                      'top-1/2 -translate-y-1/2 transform',
                      // 移动端隐藏工具提示
                      'md:block hidden'
                    )}
                  >
                    {item.label}
                    {/* 箭头 */}
                    <div className='w-2 h-2 bg-popover border-border -left-1 absolute top-1/2 -translate-y-1/2 rotate-45 transform border-b border-l' />
                  </div>
                )}
              </div>
            )
          )}
        </nav>
      </div>
    </div>
  )
}
