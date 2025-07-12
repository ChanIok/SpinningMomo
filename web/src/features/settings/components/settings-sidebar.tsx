import { cn } from '@/lib/utils'
import { Info, Settings, Zap } from 'lucide-react'
import type { SettingsSidebarItem } from '../types'
import { useSettingsStore } from '../store/settings-store'

// 侧边栏菜单项配置
const sidebarItems: SettingsSidebarItem[] = [
  {
    id: 'general',
    label: '常规',
    icon: Settings,
    description: '主题、语言、基础设置'
  },
  {
    id: 'advanced',
    label: '高级',
    icon: Zap,
    description: '调试、性能、实验性功能'
  },
  {
    id: 'about',
    label: '关于',
    icon: Info,
    description: '版本信息、许可证'
  }
]

export function SettingsSidebar() {
  const { currentPage, setCurrentPage } = useSettingsStore()

  return (
    <div className="w-64 bg-background border-r border-border flex flex-col">
      {/* 侧边栏标题 */}
      <div className="p-6 border-b border-border">
        <h2 className="text-lg font-semibold text-foreground">设置</h2>
        <p className="text-sm text-muted-foreground mt-1">
          管理应用程序首选项
        </p>
      </div>

      {/* 菜单项 */}
      <nav className="flex-1 p-4">
        <div className="space-y-2">
          {sidebarItems.map((item) => {
            const Icon = item.icon
            const isActive = currentPage === item.id

            return (
              <button
                key={item.id}
                onClick={() => setCurrentPage(item.id)}
                className={cn(
                  "w-full text-left p-3 rounded-lg transition-all duration-200",
                  "hover:bg-accent hover:text-accent-foreground",
                  "focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2",
                  "group relative",
                  {
                    "bg-primary text-primary-foreground shadow-sm": isActive,
                    "text-muted-foreground hover:text-foreground": !isActive,
                  }
                )}
              >
                <div className="flex items-start space-x-3">
                  <Icon 
                    className={cn(
                      "h-5 w-5 mt-0.5 transition-colors",
                      {
                        "text-primary-foreground": isActive,
                        "text-muted-foreground group-hover:text-foreground": !isActive,
                      }
                    )} 
                  />
                  <div className="flex-1 min-w-0">
                    <div className={cn(
                      "font-medium text-sm transition-colors",
                      {
                        "text-primary-foreground": isActive,
                        "text-foreground": !isActive,
                      }
                    )}>
                      {item.label}
                    </div>
                    {item.description && (
                      <div className={cn(
                        "text-xs mt-1 transition-colors",
                        {
                          "text-primary-foreground/80": isActive,
                          "text-muted-foreground": !isActive,
                        }
                      )}>
                        {item.description}
                      </div>
                    )}
                  </div>
                </div>

                {/* 活跃指示器 */}
                {isActive && (
                  <div className="absolute left-0 top-0 bottom-0 w-2 bg-primary-foreground rounded-r-full" />
                )}
              </button>
            )
          })}
        </div>
      </nav>

      {/* 底部信息 */}
      <div className="p-4 border-t border-border">
        <div className="text-xs text-muted-foreground">
          SpinningMomo 设置面板
        </div>
      </div>
    </div>
  )
} 