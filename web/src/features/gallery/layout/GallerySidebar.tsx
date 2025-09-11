import { ScrollArea } from '@/components/ui/scroll-area'
import { Separator } from '@/components/ui/separator'
import { Button } from '@/components/ui/button'
import { Badge } from '@/components/ui/badge'
import { Images, Tag, Settings2, ChevronRight, FolderOpen, Plus } from 'lucide-react'
import { useAssetsStore } from '@/lib/assets/assetsStore'
import { useGallerySidebar } from '../hooks'
import { useTranslation } from '@/lib/i18n'
import { cn } from '@/lib/utils'
import type { Folder } from '@/lib/assets/mockData'
// 文件夹树项组件
function FolderTreeItem({
  folder,
  level = 0,
  isSelected,
  selectedFolder,
  onToggle,
  onSelect,
  isExpanded,
  checkExpanded,
}: {
  folder: Folder
  level?: number
  isSelected: boolean
  selectedFolder: string | null
  onToggle: (id: string) => void
  onSelect: (id: string, name: string) => void
  isExpanded: boolean
  checkExpanded: (id: string) => boolean
}) {
  const toggleExpanded = () => {
    onToggle(folder.id)
  }

  const handleSelect = () => {
    onSelect(folder.id, folder.name)
  }

  return (
    <div className='space-y-1'>
      <Button
        variant={isSelected ? 'secondary' : 'ghost'}
        className={cn('h-8 w-full justify-start gap-2 px-2', isSelected && 'bg-accent')}
        style={{ paddingLeft: `${8 + level * 16}px` }}
        onClick={handleSelect}
      >
        {folder.children && (
          <div
            className='flex-shrink-0 cursor-pointer'
            onClick={(e) => {
              e.stopPropagation()
              toggleExpanded()
            }}
          >
            <ChevronRight
              className={`h-3 w-3 transition-transform ${isExpanded ? 'rotate-90' : ''}`}
            />
          </div>
        )}
        <FolderOpen className='h-3 w-3 flex-shrink-0' />
        <span className='flex-1 text-left text-sm'>{folder.name}</span>
      </Button>

      {isExpanded && folder.children && (
        <div className='space-y-1'>
          {folder.children?.map((child: Folder) => (
            <FolderTreeItem
              key={child.id}
              folder={child}
              level={level + 1}
              isSelected={selectedFolder === child.id}
              selectedFolder={selectedFolder}
              onToggle={onToggle}
              onSelect={onSelect}
              isExpanded={checkExpanded(child.id)}
              checkExpanded={checkExpanded}
            />
          ))}
        </div>
      )}
    </div>
  )
}

export function GallerySidebar() {
  const { totalCount } = useAssetsStore()
  const { t } = useTranslation()

  // 使用 gallery sidebar hook
  const sidebarHook = useGallerySidebar({ useMockData: true })
  const {
    folders,
    tags,
    sidebar,
    selectedFolder,
    selectedTag,
    selectFolder,
    selectTag,
    selectAllMedia,
    toggleFolderExpanded,
    isFolderExpanded,
    addNewTag,
  } = sidebarHook

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
        {/* 上面区域 - 媒体和文件夹 */}
        <div className='space-y-4'>
          {/* 所有媒体 */}
          <Button
            variant={sidebar.activeSection === 'all' ? 'secondary' : 'ghost'}
            className={cn(
              'h-9 w-full justify-start gap-3',
              sidebar.activeSection === 'all' && 'bg-accent'
            )}
            onClick={selectAllMedia}
          >
            <Images className='h-4 w-4 flex-shrink-0' />
            <span className='flex-1 text-left'>{t('gallery.sidebar.allItems')}</span>
            <Badge variant='secondary' className='px-1.5 py-0.5 text-xs'>
              {totalCount}
            </Badge>
          </Button>

          {/* 文件夹区域 */}
          <div className='space-y-2'>
            <h3 className='px-2 text-xs font-medium tracking-wider text-muted-foreground uppercase'>
              文件夹
            </h3>
            {folders.map((folder) => (
              <FolderTreeItem
                key={folder.id}
                folder={folder}
                isSelected={selectedFolder === folder.id}
                selectedFolder={selectedFolder}
                onToggle={toggleFolderExpanded}
                onSelect={selectFolder}
                isExpanded={isFolderExpanded(folder.id)}
                checkExpanded={isFolderExpanded}
              />
            ))}
          </div>
        </div>

        <Separator className='my-4' />

        {/* 下面区域 - 标签 */}
        <div className='space-y-2'>
          <div className='flex items-center justify-between px-2'>
            <h3 className='text-xs font-medium tracking-wider text-muted-foreground uppercase'>
              标签
            </h3>
            <Button variant='ghost' size='icon' className='h-6 w-6' onClick={addNewTag}>
              <Plus className='h-3 w-3' />
            </Button>
          </div>
          {tags.map((tag) => (
            <Button
              key={tag.id}
              variant={selectedTag === tag.id ? 'secondary' : 'ghost'}
              className={cn(
                'h-8 w-full justify-start gap-2 px-2',
                selectedTag === tag.id && 'bg-accent'
              )}
              onClick={() => selectTag(tag.id, tag.name)}
            >
              <Tag className='h-3 w-3 flex-shrink-0' />
              <span className='flex-1 text-left text-sm'>{tag.name}</span>
              <Badge variant='outline' className='px-1.5 py-0.5 text-xs'>
                {tag.count}
              </Badge>
            </Button>
          ))}
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
