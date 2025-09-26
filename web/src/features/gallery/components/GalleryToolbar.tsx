import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Separator } from '@/components/ui/separator'
import { Badge } from '@/components/ui/badge'
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuTrigger,
  DropdownMenuItem,
} from '@/components/ui/dropdown-menu'
import {
  Search,
  Grid3X3,
  Columns,
  List,
  SortAsc,
  SortDesc,
  RefreshCw,
  Plus,
  MoreVertical,
} from 'lucide-react'
import { useGalleryStore } from '@/lib/gallery/galleryStore'
import { useAssets } from '@/lib/gallery/hooks/useAssets'
import { useGalleryView, useGallerySelection } from '../hooks'
import { useTranslation } from '@/lib/i18n'
import { cn } from '@/lib/utils'

export function GalleryToolbar() {
  // 使用 gallery hooks
  const view = useGalleryView()
  const selection = useGallerySelection()
  const { refreshAssets } = useAssets()
  const { t } = useTranslation()

  // 从 store 获取必要的状态
  const { filter, sortBy, sortOrder, isLoading, totalCount, setFilter, setSorting } =
    useGalleryStore()

  // 视图模式选项
  const viewModes = [
    { id: 'masonry', icon: Columns, label: t('gallery.toolbar.viewMode.masonry') },
    { id: 'adaptive', icon: Grid3X3, label: t('gallery.toolbar.viewMode.adaptive') },
    { id: 'grid', icon: Grid3X3, label: t('gallery.toolbar.viewMode.grid') },
    { id: 'list', icon: List, label: t('gallery.toolbar.viewMode.list') },
  ] as const

  // 排序选项
  const sortOptions = [
    { id: 'created_at', label: t('gallery.toolbar.sortBy.createdAt') },
    { id: 'filename', label: t('gallery.toolbar.sortBy.filename') },
    { id: 'size', label: t('gallery.toolbar.sortBy.fileSize') },
  ] as const

  return (
    <div className='border-b bg-background/95 backdrop-blur supports-[backdrop-filter]:bg-background/60'>
      <div className='flex items-center justify-between gap-4 p-4'>
        {/* 左侧：搜索和筛选 */}
        <div className='flex items-center gap-3'>
          {/* 搜索框 */}
          <div className='relative'>
            <Search className='absolute top-1/2 left-3 h-4 w-4 -translate-y-1/2 text-muted-foreground' />
            <Input
              placeholder={t('gallery.toolbar.searchPlaceholder')}
              className='w-64 pl-9'
              value={filter.search_query || ''}
              onChange={(e) => setFilter({ ...filter, search_query: e.target.value })}
            />
          </div>

          {/* 刷新按钮 */}
          <Button variant='outline' size='icon' onClick={refreshAssets} disabled={isLoading}>
            <RefreshCw className={cn('h-4 w-4', isLoading && 'animate-spin')} />
          </Button>

          <Separator orientation='vertical' className='h-6' />

          {/* 统计信息 */}
          <div className='flex items-center gap-2 text-sm text-muted-foreground'>
            <span>{t('gallery.toolbar.totalItems', { count: totalCount })}</span>
            {selection.hasSelection && (
              <>
                <Separator orientation='vertical' className='h-4' />
                <Badge variant='secondary' className='text-xs'>
                  {t('gallery.toolbar.selectedItems', { count: selection.selectedCount })}
                </Badge>
              </>
            )}
          </div>
        </div>

        {/* 右侧：视图控制 */}
        <div className='flex items-center gap-2'>
          {/* 排序控制 */}
          <DropdownMenu>
            <DropdownMenuTrigger asChild>
              <Button variant='outline' size='sm' className='gap-2'>
                {sortOrder === 'asc' ? (
                  <SortAsc className='h-4 w-4' />
                ) : (
                  <SortDesc className='h-4 w-4' />
                )}
                {sortOptions.find((opt) => opt.id === sortBy)?.label}
              </Button>
            </DropdownMenuTrigger>
            <DropdownMenuContent align='end' className='w-40'>
              {sortOptions.map((option) => (
                <DropdownMenuItem
                  key={option.id}
                  onClick={() => setSorting(option.id, sortOrder)}
                  className={cn(sortBy === option.id && 'bg-accent')}
                >
                  {option.label}
                </DropdownMenuItem>
              ))}
              <Separator className='my-1' />
              <DropdownMenuItem
                onClick={() => setSorting(sortBy, sortOrder === 'asc' ? 'desc' : 'asc')}
              >
                {sortOrder === 'asc'
                  ? t('gallery.toolbar.sortOrder.desc')
                  : t('gallery.toolbar.sortOrder.asc')}
              </DropdownMenuItem>
            </DropdownMenuContent>
          </DropdownMenu>

          <Separator orientation='vertical' className='h-6' />

          {/* 视图模式切换 */}
          <DropdownMenu>
            <DropdownMenuTrigger asChild>
              <Button variant='outline' size='sm' className='gap-2'>
                {(() => {
                  const currentMode = viewModes.find((mode) => mode.id === view.viewConfig.mode)
                  const Icon = currentMode?.icon || Columns
                  return (
                    <>
                      <Icon className='h-4 w-4' />
                      <span className='hidden sm:inline'>{currentMode?.label}</span>
                    </>
                  )
                })()}
              </Button>
            </DropdownMenuTrigger>
            <DropdownMenuContent align='end' className='w-40'>
              {viewModes.map((mode) => {
                const Icon = mode.icon
                const isActive = view.viewConfig.mode === mode.id

                return (
                  <DropdownMenuItem
                    key={mode.id}
                    onClick={() => view.switchViewMode(mode.id)}
                    className={cn(isActive && 'bg-accent')}
                  >
                    <Icon className='mr-2 h-4 w-4' />
                    {mode.label}
                  </DropdownMenuItem>
                )
              })}
            </DropdownMenuContent>
          </DropdownMenu>

          {/* 更多操作 */}
          <DropdownMenu>
            <DropdownMenuTrigger asChild>
              <Button variant='outline' size='icon'>
                <MoreVertical className='h-4 w-4' />
              </Button>
            </DropdownMenuTrigger>
            <DropdownMenuContent align='end'>
              <DropdownMenuItem>
                <Plus className='mr-2 h-4 w-4' />
                {t('gallery.toolbar.actions.scanDirectory')}
              </DropdownMenuItem>
              <DropdownMenuItem>{t('gallery.toolbar.actions.importMedia')}</DropdownMenuItem>
              <Separator />
              <DropdownMenuItem>{t('gallery.toolbar.actions.exportSelected')}</DropdownMenuItem>
              <DropdownMenuItem>{t('gallery.toolbar.actions.cleanupThumbnails')}</DropdownMenuItem>
            </DropdownMenuContent>
          </DropdownMenu>
        </div>
      </div>
    </div>
  )
}
