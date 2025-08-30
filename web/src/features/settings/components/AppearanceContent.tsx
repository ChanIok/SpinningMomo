import { useState, useEffect } from 'react'
import { toast } from 'sonner'
import { useSettingsStore } from '@/lib/settings'
import { useAppearanceActions } from '@/features/settings/hooks/useAppearanceActions'
import { useWebSettingsStore } from '@/lib/web-settings'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Slider } from '@/components/ui/slider'
import { ResetSettingsDialog } from './ResetSettingsDialog'
import type { AppWindowLayout } from '@/lib/settings/settingsTypes'

export function AppearanceContent() {
  const { appSettings, error, isInitialized, clearError } = useSettingsStore()
  const { updateAppWindowLayout, resetAppearanceSettings } = useAppearanceActions()
  const {
    settings: webSettings,
    error: webSettingsError,
    updateBackgroundSettings,
    selectAndSetBackgroundImage,
    removeBackgroundImage,
    initialize: initializeWebSettings,
  } = useWebSettingsStore()

  // 当前布局设置状态
  const [layoutSettings, setLayoutSettings] = useState<AppWindowLayout>({
    baseItemHeight: 24,
    baseTitleHeight: 26,
    baseSeparatorHeight: 1,
    baseFontSize: 12,
    baseTextPadding: 12,
    baseIndicatorWidth: 3,
    baseRatioIndicatorWidth: 4,
    baseRatioColumnWidth: 60,
    baseResolutionColumnWidth: 120,
    baseSettingsColumnWidth: 120,
  })

  // 同步store中的布局设置到本地状态
  useEffect(() => {
    if (appSettings?.ui?.appWindowLayout) {
      setLayoutSettings(appSettings.ui.appWindowLayout)
    }
  }, [appSettings?.ui?.appWindowLayout])

  // 初始化web设置
  useEffect(() => {
    const initWebSettings = async () => {
      try {
        await initializeWebSettings()
      } catch (error) {
        console.error('Failed to initialize web settings:', error)
      }
    }
    initWebSettings()
  }, [initializeWebSettings])

  const handleInputChange = (field: keyof AppWindowLayout, value: string) => {
    const numValue = parseInt(value, 10)
    if (!isNaN(numValue) && numValue >= 0) {
      setLayoutSettings((prev) => ({
        ...prev,
        [field]: numValue,
      }))
    }
  }

  const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'Enter') {
      e.preventDefault()
      // 触发失去焦点，从而触发保存
      e.currentTarget.blur()
    }
  }

  const handleInputBlur = async (field: keyof AppWindowLayout, value: string) => {
    const numValue = parseInt(value, 10)
    if (!isNaN(numValue) && numValue >= 0) {
      const newSettings = { ...layoutSettings, [field]: numValue }
      try {
        await updateAppWindowLayout(newSettings)
      } catch (error) {
        console.error('Failed to update layout settings:', error)
        toast.error('应用外观设置失败')
      }
    }
  }

  // Web设置处理函数
  const handleOpacityChange = async (opacity: number) => {
    try {
      await updateBackgroundSettings({ opacity })
    } catch (error) {
      console.error('Failed to update opacity:', error)
      toast.error('更新背景透明度失败')
    }
  }

  const handleSelectBackgroundImage = async () => {
    try {
      await selectAndSetBackgroundImage()
    } catch (error) {
      console.error('Failed to select background image:', error)
      toast.error('设置背景图片失败')
    }
  }

  const handleRemoveBackgroundImage = async () => {
    try {
      await removeBackgroundImage()
    } catch (error) {
      console.error('Failed to remove background image:', error)
      toast.error('移除背景图片失败')
    }
  }

  const handleResetSettings = async () => {
    await resetAppearanceSettings()
    toast.success('外观设置已重置为默认值')
  }

  // 显示加载状态（仅在未初始化时）
  if (!isInitialized) {
    return (
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <div className='mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary'></div>
          <p className='mt-2 text-sm text-muted-foreground'>加载外观设置中...</p>
        </div>
      </div>
    )
  }

  // 显示错误状态
  if (error || webSettingsError) {
    return (
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <p className='text-sm text-muted-foreground'>无法加载外观设置</p>
          <p className='mt-1 text-sm text-red-500'>{error || webSettingsError}</p>
          <Button variant='outline' size='sm' onClick={clearError} className='mt-2'>
            重试
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
          <h1 className='text-2xl font-bold text-foreground'>外观设置</h1>
          <p className='mt-1 text-muted-foreground'>自定义应用程序窗口的外观参数</p>
        </div>

        <ResetSettingsDialog
          title='重置外观设置'
          description='此操作将重置当前页面设置为默认值。'
          onReset={handleResetSettings}
        />
      </div>

      <div className='space-y-8'>
        {/* 背景设置 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>背景设置</h3>
            <p className='mt-1 text-sm text-muted-foreground'>自定义应用程序的背景图片和不透明度</p>
          </div>

          <div className='space-y-4 rounded-md border border-border bg-card p-4'>
            {/* 背景不透明度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>背景不透明度</Label>
                <p className='mt-1 text-sm text-muted-foreground'>调整背景图片的不透明度</p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <div className='w-36'>
                  <Slider
                    value={[webSettings.ui.background.opacity]}
                    onValueChange={(value) => handleOpacityChange(value[0])}
                    min={0}
                    max={1}
                    step={0.1}
                    className='w-full'
                  />
                </div>
                <span className='w-12 text-sm text-muted-foreground'>
                  {(webSettings.ui.background.opacity * 100).toFixed(0)}%
                </span>
              </div>
            </div>

            {/* 背景图片控制 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>背景图片</Label>
              </div>
              <div className='flex flex-shrink-0 gap-2'>
                <Button variant='outline' size='sm' onClick={handleSelectBackgroundImage}>
                  选择图片
                </Button>
                <Button
                  variant='outline'
                  size='sm'
                  onClick={handleRemoveBackgroundImage}
                  disabled={webSettings.ui.background.type === 'none'}
                >
                  移除背景
                </Button>
              </div>
            </div>
          </div>
        </div>

        {/* 窗口外观参数 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>窗口外观参数</h3>
            <p className='mt-1 text-sm text-muted-foreground'>
              调整应用程序窗口中各元素的尺寸和间距
            </p>
          </div>

          <div className='space-y-4 rounded-md border border-border bg-card p-4'>
            {/* 基础项高度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>基础项高度</Label>
                <p className='mt-1 text-sm text-muted-foreground'>设置菜单项的基础高度（像素）</p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings.baseItemHeight}
                  onChange={(e) => handleInputChange('baseItemHeight', e.target.value)}
                  onBlur={(e) => handleInputBlur('baseItemHeight', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>px</span>
              </div>
            </div>

            {/* 标题高度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>标题高度</Label>
                <p className='mt-1 text-sm text-muted-foreground'>设置标题区域的高度（像素）</p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings.baseTitleHeight}
                  onChange={(e) => handleInputChange('baseTitleHeight', e.target.value)}
                  onBlur={(e) => handleInputBlur('baseTitleHeight', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>px</span>
              </div>
            </div>

            {/* 分隔线高度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>分隔线高度</Label>
                <p className='mt-1 text-sm text-muted-foreground'>设置分隔线的高度（像素）</p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings.baseSeparatorHeight}
                  onChange={(e) => handleInputChange('baseSeparatorHeight', e.target.value)}
                  onBlur={(e) => handleInputBlur('baseSeparatorHeight', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>px</span>
              </div>
            </div>

            {/* 基础字体大小 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>基础字体大小</Label>
                <p className='mt-1 text-sm text-muted-foreground'>设置基础字体大小（像素）</p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings.baseFontSize}
                  onChange={(e) => handleInputChange('baseFontSize', e.target.value)}
                  onBlur={(e) => handleInputBlur('baseFontSize', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>px</span>
              </div>
            </div>

            {/* 文本内边距 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>文本内边距</Label>
                <p className='mt-1 text-sm text-muted-foreground'>设置文本的内边距（像素）</p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings.baseTextPadding}
                  onChange={(e) => handleInputChange('baseTextPadding', e.target.value)}
                  onBlur={(e) => handleInputBlur('baseTextPadding', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>px</span>
              </div>
            </div>

            {/* 指示器宽度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>指示器宽度</Label>
                <p className='mt-1 text-sm text-muted-foreground'>设置指示器的宽度（像素）</p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings.baseIndicatorWidth}
                  onChange={(e) => handleInputChange('baseIndicatorWidth', e.target.value)}
                  onBlur={(e) => handleInputBlur('baseIndicatorWidth', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>px</span>
              </div>
            </div>

            {/* 比例指示器宽度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>比例指示器宽度</Label>
                <p className='mt-1 text-sm text-muted-foreground'>设置比例指示器的宽度（像素）</p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings.baseRatioIndicatorWidth}
                  onChange={(e) => handleInputChange('baseRatioIndicatorWidth', e.target.value)}
                  onBlur={(e) => handleInputBlur('baseRatioIndicatorWidth', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>px</span>
              </div>
            </div>

            {/* 比例列宽度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>比例列宽度</Label>
                <p className='mt-1 text-sm text-muted-foreground'>设置比例列的宽度（像素）</p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings.baseRatioColumnWidth}
                  onChange={(e) => handleInputChange('baseRatioColumnWidth', e.target.value)}
                  onBlur={(e) => handleInputBlur('baseRatioColumnWidth', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>px</span>
              </div>
            </div>

            {/* 分辨率列宽度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>分辨率列宽度</Label>
                <p className='mt-1 text-sm text-muted-foreground'>设置分辨率列的宽度（像素）</p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings.baseResolutionColumnWidth}
                  onChange={(e) => handleInputChange('baseResolutionColumnWidth', e.target.value)}
                  onBlur={(e) => handleInputBlur('baseResolutionColumnWidth', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>px</span>
              </div>
            </div>

            {/* 设置列宽度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>设置列宽度</Label>
                <p className='mt-1 text-sm text-muted-foreground'>设置设置列的宽度（像素）</p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings.baseSettingsColumnWidth}
                  onChange={(e) => handleInputChange('baseSettingsColumnWidth', e.target.value)}
                  onBlur={(e) => handleInputBlur('baseSettingsColumnWidth', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>px</span>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}
