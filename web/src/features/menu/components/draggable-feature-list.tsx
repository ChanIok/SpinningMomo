import React, { useState, useMemo } from 'react'
import { Switch } from '@/components/ui/switch'
import { Label } from '@/components/ui/label'
import { GripVertical } from 'lucide-react'
import type { FeatureItem } from '../types'

interface DraggableFeatureListProps {
  items: FeatureItem[]
  onReorder: (items: FeatureItem[]) => void
  onToggle: (id: string, enabled: boolean) => void
  title: string
  description: string
  className?: string
}

const DraggableFeatureListComponent = React.memo<DraggableFeatureListProps>(function DraggableFeatureList({
  items,
  onReorder,
  onToggle,
  title,
  description,
  className = ''
}) {
  const [draggedItem, setDraggedItem] = useState<string | null>(null)
  const [dragOverIndex, setDragOverIndex] = useState<number | null>(null)

  const handleDragStart = (e: React.DragEvent, item: FeatureItem) => {
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
    
    const draggedIndex = items.findIndex(item => item.id === draggedItem)
    if (draggedIndex === -1 || draggedIndex === targetIndex) return

    const newItems = [...items]
    const [removed] = newItems.splice(draggedIndex, 1)
    newItems.splice(targetIndex, 0, removed)
    
    // 重新计算order值
    const reorderedItems = newItems.map((item, index) => ({
      ...item,
      order: index + 1
    }))
    
    onReorder(reorderedItems)
    setDraggedItem(null)
  }

  const handleDragEnd = () => {
    setDraggedItem(null)
    setDragOverIndex(null)
  }

  // 使用 useMemo 优化排序操作
  const sortedItems = useMemo(() => {
    return [...items].sort((a, b) => a.order - b.order)
  }, [items])

  return (
    <div className={`space-y-4 ${className}`}>
      <div className="pb-2">
        <h3 className="text-lg font-semibold text-foreground">{title}</h3>
        <p className="text-sm text-muted-foreground mt-1">{description}</p>
      </div>
      
      <div className="border-l-2 border-border pl-4 space-y-2">
        {sortedItems.map((item, index) => (
          <FeatureListItem
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
          />
        ))}
        
        {sortedItems.length === 0 && (
          <div className="flex items-center justify-center py-8 text-center">
            <p className="text-sm text-muted-foreground">暂无功能项</p>
          </div>
        )}
      </div>
    </div>
  )
})

// 单独的列表项组件，使用 React.memo 优化
const FeatureListItem = React.memo<{
  item: FeatureItem
  index: number
  draggedItem: string | null
  dragOverIndex: number | null
  onDragStart: (e: React.DragEvent, item: FeatureItem) => void
  onDragOver: (e: React.DragEvent, index: number) => void
  onDragLeave: () => void
  onDrop: (e: React.DragEvent, targetIndex: number) => void
  onDragEnd: () => void
  onToggle: (id: string, enabled: boolean) => void
}>(function FeatureListItem({
  item,
  index,
  draggedItem,
  dragOverIndex,
  onDragStart,
  onDragOver,
  onDragLeave,
  onDrop,
  onDragEnd,
  onToggle
}) {
  return (
    <div
      draggable
      onDragStart={(e) => onDragStart(e, item)}
      onDragOver={(e) => onDragOver(e, index)}
      onDragLeave={onDragLeave}
      onDrop={(e) => onDrop(e, index)}
      onDragEnd={onDragEnd}
      className={`
        flex items-center justify-between p-3 rounded-lg border bg-card
        transition-all duration-200 cursor-move
        ${draggedItem === item.id ? 'opacity-50 scale-95' : ''}
        ${dragOverIndex === index ? 'border-primary bg-primary/5' : 'border-border'}
        hover:border-primary/50 hover:bg-accent/50
      `}
    >
      <div className="flex items-center gap-3 flex-1">
        <GripVertical className="h-4 w-4 text-muted-foreground" />
        <div className="flex-1">
          <Label className="text-sm font-medium text-foreground cursor-move">
            {item.label}
          </Label>
          <p className="text-xs text-muted-foreground mt-0.5">
            ID: {item.id}
          </p>
        </div>
      </div>
      
      <div className="flex items-center gap-2">
        <Switch
          checked={item.enabled}
          onCheckedChange={(enabled) => onToggle(item.id, enabled)}
          className="data-[state=checked]:bg-primary"
        />
        <span className="text-xs text-muted-foreground min-w-[3rem]">
          {item.enabled ? '显示' : '隐藏'}
        </span>
      </div>
    </div>
  )
})

export { DraggableFeatureListComponent as DraggableFeatureList }