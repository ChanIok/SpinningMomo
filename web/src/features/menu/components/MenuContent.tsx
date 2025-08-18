import { toast } from 'sonner'
import { useMenuActions, useMenuStore } from '../store/menuStore'
import { Button } from '@/components/ui/button'
import { DraggableFeatureList } from './DraggableFeatureList'
import { DraggablePresetList } from './DraggablePresetList'
import type { FeatureItem, PresetItem } from '../types'

export function MenuContent() {
  const { appSettings, error, isInitialized, clearError } = useMenuStore()
  const { updateFeatureItems, updateAspectRatios, updateResolutions } = useMenuActions()

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
      const updatedItems = getFeatureItems().map((item) =>
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
      const updatedItems = getAspectRatios().map((item) =>
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
      const maxOrder =
        currentItems.length > 0 ? Math.max(...currentItems.map((item) => item.order)) : 0
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
      const updatedItems = getAspectRatios().filter((item) => item.id !== id)
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
      const updatedItems = getResolutions().map((item) =>
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
      const maxOrder =
        currentItems.length > 0 ? Math.max(...currentItems.map((item) => item.order)) : 0
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
      const updatedItems = getResolutions().filter((item) => item.id !== id)
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
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <div className='mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary'></div>
          <p className='mt-2 text-sm text-muted-foreground'>加载菜单中...</p>
        </div>
      </div>
    )
  }

  // 显示错误状态
  if (error) {
    return (
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <p className='text-sm text-muted-foreground'>无法加载菜单</p>
          <p className='mt-1 text-sm text-red-500'>{error}</p>
          <Button variant='outline' size='sm' onClick={clearError} className='mt-2'>
            重试
          </Button>
        </div>
      </div>
    )
  }

  return (
    <div className='w-full max-w-[768px] p-6'>
      {/* 页面标题 */}
      <div className='mb-6'>
        <h1 className='text-2xl font-bold text-foreground'>菜单设置</h1>
        <p className='mt-1 text-muted-foreground'>管理浮窗菜单项和右键菜单项的设置</p>
      </div>

      <div className='space-y-8'>
        {/* 浮窗设置 */}
        <div className='space-y-6'>
          <div className='pb-2'>
            <h3 className='text-lg font-semibold text-foreground'>浮窗设置</h3>
            <p className='mt-1 text-sm text-muted-foreground'>自定义浮窗菜单中显示的功能和选项</p>
          </div>

          {/* 功能选择器 */}
          <DraggableFeatureList
            items={getFeatureItems()}
            onReorder={handleFeatureItemsReorder}
            onToggle={handleFeatureItemToggle}
            title='功能菜单'
            description='管理浮窗中显示的功能项，支持拖拽排序'
          />

          {/* 比例管理器 */}
          <DraggablePresetList
            items={getAspectRatios()}
            onReorder={handleAspectRatiosReorder}
            onToggle={handleAspectRatioToggle}
            onAdd={handleAspectRatioAdd}
            onRemove={handleAspectRatioRemove}
            title='宽高比设置'
            description='管理浮窗中显示的宽高比选项，支持添加自定义比例'
            addPlaceholder='如: 21:9'
            validateCustom={validateAspectRatio}
          />

          {/* 分辨率管理器 */}
          <DraggablePresetList
            items={getResolutions()}
            onReorder={handleResolutionsReorder}
            onToggle={handleResolutionToggle}
            onAdd={handleResolutionAdd}
            onRemove={handleResolutionRemove}
            title='分辨率设置'
            description='管理浮窗中显示的分辨率选项，支持添加自定义分辨率'
            addPlaceholder='如: 3840x2160 或 5K'
            validateCustom={validateResolution}
          />
        </div>
      </div>
    </div>
  )
}
