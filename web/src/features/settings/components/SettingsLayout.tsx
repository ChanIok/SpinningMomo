import { useState } from 'react'
import { cn } from '@/lib/utils'
import { Settings, Wrench, Menu, Palette } from 'lucide-react'
import { FunctionContent } from '@/features/settings/components/FunctionContent'
import { MenuContent } from '@/features/settings/components/MenuContent'
import { AppearanceContent } from '@/features/settings/components/AppearanceContent'
import { GeneralSettingsContent } from '@/features/settings/components/GeneralSettingsContent'

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

const pageTitles = {
  function: {
    title: '功能配置',
    description: '管理应用功能开关和配置选项',
  },
  menu: {
    title: '菜单管理',
    description: '自定义菜单项和布局',
  },
  appearance: {
    title: '外观主题',
    description: '个性化界面外观设置',
  },
  general: {
    title: '系统设置',
    description: '通用设置和高级选项',
  },
}

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
    <div className='flex h-full w-64 flex-col border-r border-border bg-card'>
      <div className='border-b border-border p-6'>
        <div className='flex items-center space-x-3'>
          <Settings className='h-6 w-6 text-primary' strokeWidth={2.5} />
          <h1 className='text-xl font-semibold'>设置</h1>
        </div>
      </div>

      <nav className='flex-1 p-4'>
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
                    'flex w-full items-center space-x-3 rounded-lg px-4 py-3 transition-all duration-200',
                    'text-left focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2 focus-visible:outline-none',
                    'hover:bg-accent hover:text-accent-foreground',
                    {
                      'border-r-2 border-primary bg-primary/10 text-primary': isActive,
                      'text-muted-foreground': !isActive,
                    }
                  )}
                  tabIndex={0}
                >
                  <Icon
                    className={cn('h-5 w-5 flex-shrink-0 transition-colors', {
                      'text-primary': isActive,
                      'text-muted-foreground group-hover:text-foreground': !isActive,
                    })}
                    strokeWidth={isActive ? 2.5 : 2}
                  />
                  <div className='min-w-0 flex-1'>
                    <div
                      className={cn('font-medium transition-colors', {
                        'text-primary': isActive,
                        'text-foreground group-hover:text-foreground': !isActive,
                      })}
                    >
                      {item.label}
                    </div>
                    <div
                      className={cn('text-sm transition-colors', {
                        'text-primary/70': isActive,
                        'text-muted-foreground group-hover:text-muted-foreground/80': !isActive,
                      })}
                    >
                      {item.description}
                    </div>
                  </div>
                </button>
              </div>
            )
          })}
        </div>
      </nav>
    </div>
  )
}

function SettingsMainContent({ activePage }: { activePage: SettingsPageKey }) {
  const pageInfo = pageTitles[activePage]

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
      <div className='border-b border-border bg-background/95 p-6 backdrop-blur supports-[backdrop-filter]:bg-background/60'>
        <div>
          <h2 className='text-2xl font-semibold text-foreground'>{pageInfo.title}</h2>
          <p className='mt-1 text-muted-foreground'>{pageInfo.description}</p>
        </div>
      </div>

      <div className='flex-1 overflow-auto p-6'>
        <div className='mx-auto max-w-4xl'>{renderContent()}</div>
      </div>
    </div>
  )
}

export function SettingsLayout() {
  const [activePage, setActivePage] = useState<SettingsPageKey>('function')

  return (
    <div className='flex h-full min-h-screen'>
      <SettingsSidebar activePage={activePage} setActivePage={setActivePage} />
      <SettingsMainContent activePage={activePage} />
    </div>
  )
}
