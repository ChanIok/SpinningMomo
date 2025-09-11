import { ScrollArea } from '@/components/ui/scroll-area'
import { Separator } from '@/components/ui/separator'
import { Button } from '@/components/ui/button'
import { Badge } from '@/components/ui/badge'
import { Images, FolderTree, Tag, Settings2 } from 'lucide-react'
import { useAssetsStore } from '@/lib/assets/assetsStore'
import { useTranslation } from '@/lib/i18n'
import { cn } from '@/lib/utils'

export function GallerySidebar() {
  const { sidebar, totalCount } = useAssetsStore()
  const { setSidebarActiveSection } = useAssetsStore()
  const { t } = useTranslation()

  const menuItems = [
    {
      id: 'all' as const,
      label: t('gallery.sidebar.allItems'),
      icon: Images,
      count: totalCount,
    },
    {
      id: 'folders' as const,
      label: t('gallery.sidebar.folders'),
      icon: FolderTree,
      count: undefined,
    },
    {
      id: 'tags' as const,
      label: t('gallery.sidebar.tags'),
      icon: Tag,
      count: undefined,
    },
  ]

  return (
    <div className='flex h-full flex-col border-r bg-muted/10'>
      {/* 标题栏 */}
      <div className='flex h-12 items-center justify-between border-b px-4'>
        <h2 className='text-sm font-medium'>{t('gallery.sidebar.title')}</h2>
        <Button variant='ghost' size='icon' className='h-7 w-7'>
          <Settings2 className='h-4 w-4' />
        </Button>
      </div>

      {/* 导航菜单 */}
      <ScrollArea className='flex-1 p-4'>
        <div className='space-y-2'>
          {menuItems.map((item) => {
            const Icon = item.icon
            const isActive = sidebar.activeSection === item.id

            return (
              <Button
                key={item.id}
                variant={isActive ? 'secondary' : 'ghost'}
                className={cn('h-9 w-full justify-start gap-3', isActive && 'bg-accent')}
                onClick={() => setSidebarActiveSection(item.id)}
              >
                <Icon className='h-4 w-4 flex-shrink-0' />
                <span className='flex-1 text-left'>{item.label}</span>
                {typeof item.count === 'number' && (
                  <Badge variant='secondary' className='px-1.5 py-0.5 text-xs'>
                    {item.count}
                  </Badge>
                )}
              </Button>
            )
          })}
        </div>

        <Separator className='my-4' />

        {/* 筛选器部分 - 暂时留空，后续实现 */}
        <div className='space-y-2'>
          <h3 className='px-2 text-xs font-medium tracking-wider text-muted-foreground uppercase'>
            {t('gallery.sidebar.filters')}
          </h3>
          <div className='px-2 py-4 text-center text-xs text-muted-foreground'>
            {t('gallery.sidebar.comingSoon')}
          </div>
        </div>
      </ScrollArea>

      {/* 底部信息 */}
      <div className='border-t p-4'>
        <div className='text-center text-xs text-muted-foreground'>
          {totalCount > 0
            ? t('gallery.sidebar.itemCount', { count: totalCount })
            : t('gallery.sidebar.noItems')}
        </div>
      </div>
    </div>
  )
}
