import { useState, useRef, useEffect } from 'react'
import { cn } from '@/lib/utils'
import { Settings, Wrench, Menu, Palette } from 'lucide-react'
import { FunctionContent } from '@/features/settings/components/FunctionContent'
import { MenuContent } from '@/features/settings/components/MenuContent'
import { AppearanceContent } from '@/features/settings/components/AppearanceContent'
import { GeneralSettingsContent } from '@/features/settings/components/GeneralSettingsContent'
import { ScrollArea } from '@/components/ui/scroll-area'

type SettingsPageKey = 'function' | 'menu' | 'appearance' | 'general'

interface SettingsMenuItem {
  key: SettingsPageKey
  label: string
  icon: React.ElementType
  description: string
}

const settingsMenus: SettingsMenuItem[] = [
  {
    key: 'function',
    label: '功能配置',
    icon: Wrench,
    description: '管理应用功能开关和配置',
  },
  {
    key: 'menu',
    label: '菜单管理',
    icon: Menu,
    description: '自定义菜单项和布局',
  },
  {
    key: 'appearance',
    label: '外观主题',
    icon: Palette,
    description: '个性化界面外观设置',
  },
  {
    key: 'general',
    label: '系统设置',
    icon: Settings,
    description: '通用设置和高级选项',
  },
]

function SettingsSidebar({
  activePage,
  setActivePage,
}: {
  activePage: SettingsPageKey
  setActivePage: (page: SettingsPageKey) => void
}) {
  const handleMenuClick = (key: SettingsPageKey) => {
    setActivePage(key)
  }

  const handleKeyDown = (event: React.KeyboardEvent, key: SettingsPageKey) => {
    if (event.key === 'Enter' || event.key === ' ') {
      event.preventDefault()
      handleMenuClick(key)
    }
  }

  return (
    <div className='flex h-full w-48 flex-col lg:w-56 2xl:w-64'>
      <div className='h-full p-4'>
        <nav className='flex-1'>
          <div className='space-y-1'>
            {settingsMenus.map((item) => {
              const isActive = activePage === item.key
              const Icon = item.icon

              return (
                <div key={item.key} className='group'>
                  <button
                    onClick={() => handleMenuClick(item.key)}
                    onKeyDown={(e) => handleKeyDown(e, item.key)}
                    className={cn(
                      'flex w-full items-center space-x-3 rounded-md px-4 py-3 transition-all duration-200',
                      'text-left focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2 focus-visible:outline-none',
                      'hover:bg-sidebar-accent hover:text-sidebar-accent-foreground',
                      {
                        'bg-sidebar-accent text-sidebar-accent-foreground': isActive,
                        'text-sidebar-foreground': !isActive,
                      }
                    )}
                    tabIndex={0}
                  >
                    <Icon
                      className={cn('h-5 w-5 flex-shrink-0 transition-colors', {
                        'text-sidebar-accent-foreground': isActive,
                        'text-sidebar-foreground group-hover:text-sidebar-primary': !isActive,
                      })}
                      strokeWidth={isActive ? 2.5 : 1.8}
                    />
                    <div className='min-w-0 flex-1'>
                      <div
                        className={cn('font-medium transition-colors', {
                          'text-sidebar-accent-foreground': isActive,
                          'text-sidebar-foreground group-hover:text-sidebar-primary': !isActive,
                        })}
                      >
                        {item.label}
                      </div>
                    </div>
                  </button>
                </div>
              )
            })}
          </div>
        </nav>
      </div>
    </div>
  )
}

function SettingsMainContent({ activePage }: { activePage: SettingsPageKey }) {
  const scrollAreaRef = useRef<HTMLDivElement>(null)

  // 当页面切换时，重置滚动位置到顶部
  useEffect(() => {
    if (scrollAreaRef.current) {
      const scrollableElement = scrollAreaRef.current.querySelector(
        '[data-radix-scroll-area-viewport]'
      )
      if (scrollableElement) {
        scrollableElement.scrollTo({ top: 0, behavior: 'smooth' })
      }
    }
  }, [activePage])

  const renderContent = () => {
    switch (activePage) {
      case 'function':
        return <FunctionContent />
      case 'menu':
        return <MenuContent />
      case 'appearance':
        return <AppearanceContent />
      case 'general':
        return <GeneralSettingsContent />
      default:
        return <FunctionContent />
    }
  }

  return (
    <div className='flex h-full flex-1 flex-col overflow-hidden'>
      <ScrollArea ref={scrollAreaRef} className='flex-1 overflow-auto'>
        <div className='mx-auto max-w-4xl p-4'>{renderContent()}</div>
      </ScrollArea>
    </div>
  )
}

export function SettingsLayout() {
  const [activePage, setActivePage] = useState<SettingsPageKey>('function')

  return (
    <div className='flex h-full'>
      <SettingsSidebar activePage={activePage} setActivePage={setActivePage} />
      <SettingsMainContent activePage={activePage} />
    </div>
  )
}
