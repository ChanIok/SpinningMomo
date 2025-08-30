import { useState, useRef, useEffect } from 'react'
import { cn } from '@/lib/utils'
import { Settings, Wrench, Menu, Palette } from 'lucide-react'
import { FunctionContent } from '@/features/settings/components/FunctionContent'
import { MenuContent } from '@/features/settings/components/MenuContent'
import { AppearanceContent } from '@/features/settings/components/AppearanceContent'
import { GeneralSettingsContent } from '@/features/settings/components/GeneralSettingsContent'
import { ScrollArea } from '@/components/ui/scroll-area'
import { useTranslation } from '@/lib/i18n'

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
    label: 'settings.layout.function.title',
    icon: Wrench,
    description: 'settings.layout.function.description',
  },
  {
    key: 'menu',
    label: 'settings.layout.menu.title',
    icon: Menu,
    description: 'settings.layout.menu.description',
  },
  {
    key: 'appearance',
    label: 'settings.layout.appearance.title',
    icon: Palette,
    description: 'settings.layout.appearance.description',
  },
  {
    key: 'general',
    label: 'settings.layout.general.title',
    icon: Settings,
    description: 'settings.layout.general.description',
  },
]

function SettingsSidebar({
  activePage,
  setActivePage,
}: {
  activePage: SettingsPageKey
  setActivePage: (page: SettingsPageKey) => void
}) {
  const { t } = useTranslation()
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
                    title={t(item.description)}
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
                        {t(item.label)}
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
