import { toast } from 'sonner'
import { useSettingsStore } from '@/lib/settings'
import { useMenuActions } from '@/features/settings/hooks/useMenuActions'
import { Button } from '@/components/ui/button'
import { DraggableFeatureList } from './DraggableFeatureList'
import { DraggablePresetList } from './DraggablePresetList'
import { ResetSettingsDialog } from './ResetSettingsDialog'
import { useTranslation } from '@/lib/i18n'
import type { FeatureItem, PresetItem } from '@/lib/settings/settingsTypes'

export function MenuContent() {
  const { t } = useTranslation()
  const { appSettings, error, isInitialized, clearError } = useSettingsStore()
  const { updateFeatureItems, updateAspectRatios, updateResolutions, resetMenuSettings } =
    useMenuActions()

  // 安全获取菜单数据的辅助函数（适配新的嵌套结构）
  const getFeatureItems = () => appSettings?.ui?.appMenu?.featureItems || []
  const getAspectRatios = () => appSettings?.ui?.appMenu?.aspectRatios || []
  const getResolutions = () => appSettings?.ui?.appMenu?.resolutions || []

  // 功能项相关处理
  const handleFeatureItemsReorder = async (items: FeatureItem[]) => {
    try {
      await updateFeatureItems(items)
    } catch (error) {
      console.error('Failed to update feature items:', error)
      toast.error(t('settings.menu.feature.updateFailed'))
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
      toast.error(t('settings.menu.feature.updateFailed'))
    }
  }

  // 比例设置相关处理
  const handleAspectRatiosReorder = async (items: PresetItem[]) => {
    try {
      await updateAspectRatios(items)
    } catch (error) {
      console.error('Failed to update aspect ratios:', error)
      toast.error(t('settings.menu.aspectRatio.updateFailed'))
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
      toast.error(t('settings.menu.aspectRatio.updateFailed'))
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
    } catch (error) {
      console.error('Failed to add aspect ratio:', error)
      toast.error(t('settings.menu.aspectRatio.addFailed'))
    }
  }

  const handleAspectRatioRemove = async (id: string) => {
    try {
      const updatedItems = getAspectRatios().filter((item) => item.id !== id)
      await updateAspectRatios(updatedItems)
    } catch (error) {
      console.error('Failed to remove aspect ratio:', error)
      toast.error(t('settings.menu.aspectRatio.removeFailed'))
    }
  }

  // 分辨率设置相关处理
  const handleResolutionsReorder = async (items: PresetItem[]) => {
    try {
      await updateResolutions(items)
    } catch (error) {
      console.error('Failed to update resolutions:', error)
      toast.error(t('settings.menu.resolution.updateFailed'))
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
      toast.error(t('settings.menu.resolution.updateFailed'))
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
    } catch (error) {
      console.error('Failed to add resolution:', error)
      toast.error(t('settings.menu.resolution.addFailed'))
    }
  }

  const handleResolutionRemove = async (id: string) => {
    try {
      const updatedItems = getResolutions().filter((item) => item.id !== id)
      await updateResolutions(updatedItems)
    } catch (error) {
      console.error('Failed to remove resolution:', error)
      toast.error(t('settings.menu.resolution.removeFailed'))
    }
  }

  const handleResetSettings = async () => {
    await resetMenuSettings()
    toast.success(t('settings.menu.reset.success'))
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
          <p className='mt-2 text-sm text-muted-foreground'>{t('settings.menu.loading')}</p>
        </div>
      </div>
    )
  }

  // 显示错误状态
  if (error) {
    return (
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <p className='text-sm text-muted-foreground'>{t('settings.menu.error.title')}</p>
          <p className='mt-1 text-sm text-red-500'>{error}</p>
          <Button variant='outline' size='sm' onClick={clearError} className='mt-2'>
            {t('settings.menu.error.retry')}
          </Button>
        </div>
      </div>
    )
  }

  return (
    <div className='w-full'>
      {/* 页面标题 */}
      <div className='mb-6 flex items-center justify-between'>
        <div>
          <h1 className='text-2xl font-bold text-foreground'>{t('settings.menu.title')}</h1>
          <p className='mt-1 text-muted-foreground'>{t('settings.menu.description')}</p>
        </div>

        <ResetSettingsDialog
          title={t('settings.menu.reset.title')}
          description={t('settings.menu.reset.description')}
          onReset={handleResetSettings}
        />
      </div>

      <div className='space-y-8'>
        {/* 功能选择器 */}
        <DraggableFeatureList
          items={getFeatureItems()}
          onReorder={handleFeatureItemsReorder}
          onToggle={handleFeatureItemToggle}
          title={t('settings.menu.feature.title')}
          description={t('settings.menu.feature.description')}
        />

        {/* 比例管理器 */}
        <DraggablePresetList
          items={getAspectRatios()}
          onReorder={handleAspectRatiosReorder}
          onToggle={handleAspectRatioToggle}
          onAdd={handleAspectRatioAdd}
          onRemove={handleAspectRatioRemove}
          title={t('settings.menu.aspectRatio.title')}
          description={t('settings.menu.aspectRatio.description')}
          addPlaceholder={t('settings.menu.aspectRatio.placeholder')}
          validateCustom={validateAspectRatio}
        />

        {/* 分辨率管理器 */}
        <DraggablePresetList
          items={getResolutions()}
          onReorder={handleResolutionsReorder}
          onToggle={handleResolutionToggle}
          onAdd={handleResolutionAdd}
          onRemove={handleResolutionRemove}
          title={t('settings.menu.resolution.title')}
          description={t('settings.menu.resolution.description')}
          addPlaceholder={t('settings.menu.resolution.placeholder')}
          validateCustom={validateResolution}
        />
      </div>
    </div>
  )
}
