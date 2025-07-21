import { useState, useEffect } from 'react'
import { toast } from 'sonner'
import { useMenuStore } from '../store/menu-store'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { DraggableFeatureList } from './draggable-feature-list'
import { DraggablePresetList } from './draggable-preset-list'
import type { FeatureItem, PresetItem } from '../types'

export function MenuContent() {
  const {
    appSettings,
    error,
    isInitialized,
    updateWindowTitle,
    updateFeatureItems,
    updateAspectRatios,
    updateResolutions,
    clearError
  } = useMenuStore()

  const [inputTitle, setInputTitle] = useState('')
  const [isUpdatingTitle, setIsUpdatingTitle] = useState(false)

  // 同步store中的标题到输入框（适配新的嵌套结构）
  useEffect(() => {
    setInputTitle(appSettings?.window?.targetTitle || '')
  }, [appSettings?.window?.targetTitle])

  const handleUpdateTitle = async () => {
    if (inputTitle.trim() === '') {
      toast.error('窗口标题不能为空')
      return
    }

    setIsUpdatingTitle(true)
    try {
      await updateWindowTitle(inputTitle.trim())
      toast.success('窗口标题已更新')
    } catch (error) {
      console.error('Failed to update window title:', error)
      toast.error('更新窗口标题失败')
    } finally {
      setIsUpdatingTitle(false)
    }
  }

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') {
      handleUpdateTitle()
    }
  }

  // 安全获取菜单数据的辅助函数（适配新的嵌套结构）
  const getFeatureItems = () => appSettings?.ui?.appMenu?.featureItems || []
  const getAspectRatios = () => appSettings?.ui?.appMenu?.aspectRatios || []
  const getResolutions = () => appSettings?.ui?.appMenu?.resolutions || []

  // 功能项相关处理
  const handleFeatureItemsReorder = async (items: FeatureItem[]) => {
    try {
      await updateFeatureItems(items)
      toast.success('功能项顺序已更新')
    } catch (error) {
      console.error('Failed to update feature items:', error)
      toast.error('更新功能项失败')
    }
  }

  const handleFeatureItemToggle = async (id: string, enabled: boolean) => {
    try {
      const updatedItems = getFeatureItems().map(item =>
        item.id === id ? { ...item, enabled } : item
      )
      await updateFeatureItems(updatedItems)
      // 乐观更新，无需立即显示成功提示，只在失败时显示错误
    } catch (error) {
      console.error('Failed to toggle feature item:', error)
      toast.error('更新功能项失败')
    }
  }

  // 比例设置相关处理
  const handleAspectRatiosReorder = async (items: PresetItem[]) => {
    try {
      await updateAspectRatios(items)
      toast.success('比例顺序已更新')
    } catch (error) {
      console.error('Failed to update aspect ratios:', error)
      toast.error('更新比例设置失败')
    }
  }

  const handleAspectRatioToggle = async (id: string, enabled: boolean) => {
    try {
      const updatedItems = getAspectRatios().map(item =>
        item.id === id ? { ...item, enabled } : item
      )
      await updateAspectRatios(updatedItems)
      // 乐观更新，无需立即显示成功提示
    } catch (error) {
      console.error('Failed to toggle aspect ratio:', error)
      toast.error('更新比例设置失败')
    }
  }

  const handleAspectRatioAdd = async (newItem: Omit<PresetItem, 'order'>) => {
    try {
      const currentItems = getAspectRatios()
      const maxOrder = currentItems.length > 0 ? Math.max(...currentItems.map(item => item.order)) : 0
      const itemWithOrder = { ...newItem, order: maxOrder + 1 }
      const updatedItems = [...currentItems, itemWithOrder]
      await updateAspectRatios(updatedItems)
      toast.success('比例已添加')
    } catch (error) {
      console.error('Failed to add aspect ratio:', error)
      toast.error('添加比例失败')
    }
  }

  const handleAspectRatioRemove = async (id: string) => {
    try {
      const updatedItems = getAspectRatios().filter(item => item.id !== id)
      await updateAspectRatios(updatedItems)
      toast.success('比例已删除')
    } catch (error) {
      console.error('Failed to remove aspect ratio:', error)
      toast.error('删除比例失败')
    }
  }

  // 分辨率设置相关处理
  const handleResolutionsReorder = async (items: PresetItem[]) => {
    try {
      await updateResolutions(items)
      toast.success('分辨率顺序已更新')
    } catch (error) {
      console.error('Failed to update resolutions:', error)
      toast.error('更新分辨率设置失败')
    }
  }

  const handleResolutionToggle = async (id: string, enabled: boolean) => {
    try {
      const updatedItems = getResolutions().map(item =>
        item.id === id ? { ...item, enabled } : item
      )
      await updateResolutions(updatedItems)
      // 乐观更新，无需立即显示成功提示
    } catch (error) {
      console.error('Failed to toggle resolution:', error)
      toast.error('更新分辨率设置失败')
    }
  }

  const handleResolutionAdd = async (newItem: Omit<PresetItem, 'order'>) => {
    try {
      const currentItems = getResolutions()
      const maxOrder = currentItems.length > 0 ? Math.max(...currentItems.map(item => item.order)) : 0
      const itemWithOrder = { ...newItem, order: maxOrder + 1 }
      const updatedItems = [...currentItems, itemWithOrder]
      await updateResolutions(updatedItems)
      toast.success('分辨率已添加')
    } catch (error) {
      console.error('Failed to add resolution:', error)
      toast.error('添加分辨率失败')
    }
  }

  const handleResolutionRemove = async (id: string) => {
    try {
      const updatedItems = getResolutions().filter(item => item.id !== id)
      await updateResolutions(updatedItems)
      toast.success('分辨率已删除')
    } catch (error) {
      console.error('Failed to remove resolution:', error)
      toast.error('删除分辨率失败')
    }
  }

  // 验证函数
  const validateAspectRatio = (value: string): boolean => {
    const regex = /^\d+:\d+$/
    return regex.test(value) && !value.includes('0:') && !value.includes(':0')
  }

  const validateResolution = (value: string): boolean => {
    // 支持 1920x1080 格式或预设名称
    const resolutionRegex = /^\d+x\d+$/
    const presetRegex = /^\d+[KkPp]?$/
    return resolutionRegex.test(value) || presetRegex.test(value)
  }

  // 显示加载状态（仅在未初始化时）
  if (!isInitialized) {
    return (
      <div className="p-6 flex items-center justify-center">
        <div className="text-center">
          <div className="h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary mx-auto"></div>
          <p className="text-sm text-muted-foreground mt-2">加载菜单中...</p>
        </div>
      </div>
    )
  }

  // 显示错误状态
  if (error) {
    return (
      <div className="p-6 flex items-center justify-center">
        <div className="text-center">
          <p className="text-sm text-muted-foreground">无法加载菜单</p>
          <p className="text-sm text-red-500 mt-1">{error}</p>
          <Button 
            variant="outline" 
            size="sm" 
            onClick={clearError}
            className="mt-2"
          >
            重试
          </Button>
        </div>
      </div>
    )
  }

  return (
    <div className="p-6 max-w-4xl">
      {/* 页面标题 */}
      <div className="mb-6">
        <h1 className="text-2xl font-bold text-foreground">菜单</h1>
        <p className="text-muted-foreground mt-1">
          管理应用程序的各种功能和设置
        </p>
      </div>

      <div className="space-y-8">
        {/* 窗口控制 */}
        <div className="space-y-4">
          <div className="pb-2">
            <h3 className="text-lg font-semibold text-foreground">窗口控制</h3>
            <p className="text-sm text-muted-foreground mt-1">
              自定义应用程序窗口的显示设置
            </p>
          </div>
          
          <div className="border-l-2 border-border pl-4 space-y-4">
            <div className="flex items-center justify-between py-4">
              <div className="flex-1 pr-4">
                <Label className="text-sm font-medium text-foreground">
                  窗口标题
                </Label>
                <p className="text-sm text-muted-foreground mt-1">
                  设置应用程序窗口的标题栏文本
                </p>
              </div>
              <div className="flex-shrink-0 flex items-center gap-2">
                <Input
                  value={inputTitle}
                  onChange={(e) => setInputTitle(e.target.value)}
                  onKeyDown={handleKeyDown}
                  placeholder="输入窗口标题..."
                  className="w-48"
                  disabled={isUpdatingTitle}
                />
                <Button
                  onClick={handleUpdateTitle}
                  disabled={isUpdatingTitle || inputTitle.trim() === ''}
                  size="sm"
                >
                  {isUpdatingTitle ? '更新中...' : '更新'}
                </Button>
              </div>
            </div>
          </div>
        </div>

        {/* 浮窗设置 */}
        <div className="space-y-6">
          <div className="pb-2">
            <h3 className="text-lg font-semibold text-foreground">浮窗设置</h3>
            <p className="text-sm text-muted-foreground mt-1">
              自定义浮窗菜单中显示的功能和选项
            </p>
          </div>

          {/* 功能选择器 */}
          <DraggableFeatureList
            items={getFeatureItems()}
            onReorder={handleFeatureItemsReorder}
            onToggle={handleFeatureItemToggle}
            title="功能菜单"
            description="管理浮窗中显示的功能项，支持拖拽排序"
          />

          {/* 比例管理器 */}
          <DraggablePresetList
            items={getAspectRatios()}
            onReorder={handleAspectRatiosReorder}
            onToggle={handleAspectRatioToggle}
            onAdd={handleAspectRatioAdd}
            onRemove={handleAspectRatioRemove}
            title="宽高比设置"
            description="管理浮窗中显示的宽高比选项，支持添加自定义比例"
            addPlaceholder="如: 21:9"
            validateCustom={validateAspectRatio}
          />

          {/* 分辨率管理器 */}
          <DraggablePresetList
            items={getResolutions()}
            onReorder={handleResolutionsReorder}
            onToggle={handleResolutionToggle}
            onAdd={handleResolutionAdd}
            onRemove={handleResolutionRemove}
            title="分辨率设置"
            description="管理浮窗中显示的分辨率选项，支持添加自定义分辨率"
            addPlaceholder="如: 3840x2160 或 5K"
            validateCustom={validateResolution}
          />
        </div>
      </div>
    </div>
  )
} 