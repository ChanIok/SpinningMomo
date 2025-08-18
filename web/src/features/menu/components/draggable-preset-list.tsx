import React, { useState, useMemo } from 'react'
import { Switch } from '@/components/ui/switch'
import { Label } from '@/components/ui/label'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { GripVertical, Plus, Trash2 } from 'lucide-react'
import { toast } from 'sonner'
import type { PresetItem } from '../types'

// 硬编码的预设项标签映射（比例和分辨率共用id作为label）
const getPresetItemLabel = (id: string): string => {
  return id
}

interface DraggablePresetListProps {
  items: PresetItem[]
  onReorder: (items: PresetItem[]) => void
  onToggle: (id: string, enabled: boolean) => void
  onAdd: (item: Omit<PresetItem, 'order'>) => void
  onRemove: (id: string) => void
  title: string
  description: string
  addPlaceholder: string
  validateCustom?: (value: string) => boolean
  className?: string
}

const DraggablePresetListComponent = React.memo<DraggablePresetListProps>(
  function DraggablePresetList({
    items,
    onReorder,
    onToggle,
    onAdd,
    onRemove,
    title,
    description,
    addPlaceholder,
    validateCustom,
    className = '',
  }) {
    const [draggedItem, setDraggedItem] = useState<string | null>(null)
    const [dragOverIndex, setDragOverIndex] = useState<number | null>(null)
    const [newItemValue, setNewItemValue] = useState('')
    const [isAdding, setIsAdding] = useState(false)

    const handleDragStart = (e: React.DragEvent, item: PresetItem) => {
      setDraggedItem(item.id)
      e.dataTransfer.effectAllowed = 'move'
      e.dataTransfer.setData('text/plain', item.id)
    }

    const handleDragOver = (e: React.DragEvent, index: number) => {
      e.preventDefault()
      e.dataTransfer.dropEffect = 'move'
      setDragOverIndex(index)
    }

    const handleDragLeave = () => {
      setDragOverIndex(null)
    }

    const handleDrop = (e: React.DragEvent, targetIndex: number) => {
      e.preventDefault()
      setDragOverIndex(null)

      if (!draggedItem) return

      const draggedIndex = items.findIndex((item) => item.id === draggedItem)
      if (draggedIndex === -1 || draggedIndex === targetIndex) return

      const newItems = [...items]
      const [removed] = newItems.splice(draggedIndex, 1)
      newItems.splice(targetIndex, 0, removed)

      // 重新计算order值
      const reorderedItems = newItems.map((item, index) => ({
        ...item,
        order: index + 1,
      }))

      onReorder(reorderedItems)
      setDraggedItem(null)
    }

    const handleDragEnd = () => {
      setDraggedItem(null)
      setDragOverIndex(null)
    }

    const handleAddItem = () => {
      const value = newItemValue.trim()
      if (!value) {
        toast.error('请输入有效值')
        return
      }

      // 检查是否已存在
      if (items.some((item) => item.id === value)) {
        toast.error('该项已存在')
        return
      }

      // 自定义验证
      if (validateCustom && !validateCustom(value)) {
        toast.error('格式不正确')
        return
      }

      const newItem: Omit<PresetItem, 'order'> = {
        id: value,
        enabled: true,
      }

      onAdd(newItem)
      setNewItemValue('')
      setIsAdding(false)
      toast.success('添加成功')
    }

    const handleRemove = (id: string) => {
      onRemove(id)
      toast.success('删除成功')
    }

    const handleKeyDown = (e: React.KeyboardEvent) => {
      if (e.key === 'Enter') {
        handleAddItem()
      } else if (e.key === 'Escape') {
        setIsAdding(false)
        setNewItemValue('')
      }
    }

    // 使用 useMemo 优化排序操作
    const sortedItems = useMemo(() => {
      return [...items].sort((a, b) => a.order - b.order)
    }, [items])

    return (
      <div className={`space-y-4 ${className}`}>
        <div className='pb-2'>
          <h3 className='text-lg font-semibold text-foreground'>{title}</h3>
          <p className='mt-1 text-sm text-muted-foreground'>{description}</p>
        </div>

        <div className='space-y-2 border-l-2 border-border pl-4'>
          {sortedItems.map((item, index) => (
            <PresetListItem
              key={item.id}
              item={item}
              index={index}
              draggedItem={draggedItem}
              dragOverIndex={dragOverIndex}
              onDragStart={handleDragStart}
              onDragOver={handleDragOver}
              onDragLeave={handleDragLeave}
              onDrop={handleDrop}
              onDragEnd={handleDragEnd}
              onToggle={onToggle}
              onRemove={handleRemove}
            />
          ))}

          {/* 添加新项 */}
          {isAdding ? (
            <div className='flex items-center gap-2 rounded-lg border border-primary bg-primary/5 p-3'>
              <Input
                value={newItemValue}
                onChange={(e) => setNewItemValue(e.target.value)}
                onKeyDown={handleKeyDown}
                placeholder={addPlaceholder}
                className='flex-1'
                autoFocus
              />
              <Button size='sm' onClick={handleAddItem}>
                添加
              </Button>
              <Button
                variant='ghost'
                size='sm'
                onClick={() => {
                  setIsAdding(false)
                  setNewItemValue('')
                }}
              >
                取消
              </Button>
            </div>
          ) : (
            <Button
              variant='outline'
              size='sm'
              onClick={() => setIsAdding(true)}
              className='w-full border-dashed hover:border-primary hover:text-primary'
            >
              <Plus className='mr-2 h-4 w-4' />
              添加自定义项
            </Button>
          )}

          {sortedItems.length === 0 && !isAdding && (
            <div className='flex items-center justify-center py-8 text-center'>
              <p className='text-sm text-muted-foreground'>暂无预设项</p>
            </div>
          )}
        </div>
      </div>
    )
  }
)

// 单独的列表项组件，使用 React.memo 优化
const PresetListItem = React.memo<{
  item: PresetItem
  index: number
  draggedItem: string | null
  dragOverIndex: number | null
  onDragStart: (e: React.DragEvent, item: PresetItem) => void
  onDragOver: (e: React.DragEvent, index: number) => void
  onDragLeave: () => void
  onDrop: (e: React.DragEvent, targetIndex: number) => void
  onDragEnd: () => void
  onToggle: (id: string, enabled: boolean) => void
  onRemove: (id: string) => void
}>(function PresetListItem({
  item,
  index,
  draggedItem,
  dragOverIndex,
  onDragStart,
  onDragOver,
  onDragLeave,
  onDrop,
  onDragEnd,
  onToggle,
  onRemove,
}) {
  return (
    <div
      draggable
      onDragStart={(e) => onDragStart(e, item)}
      onDragOver={(e) => onDragOver(e, index)}
      onDragLeave={onDragLeave}
      onDrop={(e) => onDrop(e, index)}
      onDragEnd={onDragEnd}
      className={`flex cursor-move items-center justify-between rounded-lg border bg-card p-3 transition-all duration-200 ${draggedItem === item.id ? 'scale-95 opacity-50' : ''} ${dragOverIndex === index ? 'border-primary bg-primary/5' : 'border-border'} hover:border-primary/50 hover:bg-accent/50`}
    >
      <div className='flex flex-1 items-center gap-3'>
        <GripVertical className='h-4 w-4 text-muted-foreground' />
        <div className='flex-1'>
          <Label className='cursor-move text-sm font-medium text-foreground'>
            {getPresetItemLabel(item.id)}
          </Label>
        </div>
      </div>

      <div className='flex items-center gap-2'>
        <Switch
          checked={item.enabled}
          onCheckedChange={(enabled) => onToggle(item.id, enabled)}
          className='data-[state=checked]:bg-primary'
        />
        <span className='min-w-[3rem] text-xs text-muted-foreground'>
          {item.enabled ? '显示' : '隐藏'}
        </span>
        <Button
          variant='ghost'
          size='sm'
          onClick={() => onRemove(item.id)}
          className='h-8 w-8 p-0 text-destructive hover:bg-destructive/10 hover:text-destructive'
        >
          <Trash2 className='h-3 w-3' />
        </Button>
      </div>
    </div>
  )
})

export { DraggablePresetListComponent as DraggablePresetList }
