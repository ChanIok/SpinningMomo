import { useEffect } from 'react'
import { toast } from 'sonner'
import { useSettingsStore } from '@/lib/settings'
import { useAppearanceActions } from '@/features/settings/hooks/useAppearanceActions'
import { useWebSettingsStore } from '@/lib/web-settings'
import { useTheme } from 'next-themes'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Slider } from '@/components/ui/slider'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { ResetSettingsDialog } from './ResetSettingsDialog'
import { useTranslation } from '@/lib/i18n'
import type { AppWindowLayout, AppWindowThemeMode } from '@/lib/settings/settingsTypes'

export function AppearanceContent() {
  const { t } = useTranslation()
  const { appSettings, error, isInitialized, clearError } = useSettingsStore()
  const { webSettings } = useWebSettingsStore()
  const {
    updateAppWindowLayout,
    resetAppearanceSettings,
    updateBackgroundOpacity,
    updateBackgroundBlur,
    handleBackgroundImageSelect,
    handleBackgroundImageRemove,
    updateAppWindowTheme,
  } = useAppearanceActions()
  const { error: webSettingsError, initialize: initializeWebSettings } = useWebSettingsStore()
  const { theme, setTheme } = useTheme()

  const layoutSettings = appSettings?.ui?.appWindowLayout
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

  const handleLayoutChange = async (field: keyof AppWindowLayout, value: string) => {
    const numValue = parseInt(value, 10)
    if (!isNaN(numValue) && numValue >= 0 && layoutSettings) {
      try {
        await updateAppWindowLayout({
          ...layoutSettings,
          [field]: numValue,
        })
      } catch (error) {
        console.error('Failed to update layout settings:', error)
        toast.error(t('settings.appearance.layout.updateFailed'))
      }
    }
  }

  const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'Enter') {
      e.preventDefault()
      // 触发失去焦点，从而触发保存
      e.currentTarget.blur()
    }
  }

  // Web设置处理函数
  const handleOpacityChange = async (opacity: number) => {
    try {
      await updateBackgroundOpacity(opacity)
    } catch (error) {
      console.error('Failed to update opacity:', error)
      toast.error(t('settings.appearance.background.updateFailed'))
    }
  }

  const handleBlurAmountChange = async (blurAmount: number) => {
    try {
      await updateBackgroundBlur(blurAmount)
    } catch (error) {
      console.error('Failed to update blur amount:', error)
      toast.error(t('settings.appearance.background.updateFailed'))
    }
  }

  const handleSelectBackgroundImage = async () => {
    try {
      await handleBackgroundImageSelect()
    } catch (error) {
      console.error('Failed to select background image:', error)
      toast.error(t('settings.appearance.background.selectFailed'))
    }
  }

  const handleRemoveBackgroundImage = async () => {
    try {
      await handleBackgroundImageRemove()
    } catch (error) {
      console.error('Failed to remove background image:', error)
      toast.error(t('settings.appearance.background.removeFailed'))
    }
  }

  // 主题选项
  const themeOptions = [
    { value: 'light', label: t('settings.appearance.theme.light') },
    { value: 'dark', label: t('settings.appearance.theme.dark') },
    { value: 'system', label: t('settings.appearance.theme.system') },
  ]

  // AppWindow主题选项
  const appWindowThemeOptions = [
    { value: 'light', label: t('settings.appearance.appWindowTheme.light') },
    { value: 'dark', label: t('settings.appearance.appWindowTheme.dark') },
  ]

  // 主题切换处理
  const handleThemeChange = (themeMode: 'light' | 'dark' | 'system') => {
    setTheme(themeMode)
  }

  // AppWindow主题切换处理
  const handleAppWindowThemeChange = async (themeMode: AppWindowThemeMode) => {
    try {
      await updateAppWindowTheme(themeMode)
    } catch (error) {
      console.error('Failed to update AppWindow theme:', error)
      toast.error(t('settings.appearance.appWindowTheme.updateFailed'))
    }
  }

  const handleResetSettings = async () => {
    await resetAppearanceSettings()
    toast.success(t('settings.appearance.reset.success'))
  }

  // 显示加载状态（仅在未初始化时）
  if (!isInitialized) {
    return (
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <div className='mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary'></div>
          <p className='mt-2 text-sm text-muted-foreground'>{t('settings.appearance.loading')}</p>
        </div>
      </div>
    )
  }

  // 显示错误状态
  if (error || webSettingsError) {
    return (
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <p className='text-sm text-muted-foreground'>{t('settings.appearance.error.title')}</p>
          <p className='mt-1 text-sm text-red-500'>{error || webSettingsError}</p>
          <Button variant='outline' size='sm' onClick={clearError} className='mt-2'>
            {t('settings.appearance.error.retry')}
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
          <h1 className='text-2xl font-bold text-foreground'>{t('settings.appearance.title')}</h1>
          <p className='mt-1 text-muted-foreground'>{t('settings.appearance.description')}</p>
        </div>

        <ResetSettingsDialog
          title={t('settings.appearance.reset.title')}
          description={t('settings.appearance.reset.description')}
          onReset={handleResetSettings}
        />
      </div>

      <div className='space-y-8'>
        {/* 背景设置 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>
              {t('settings.appearance.background.title')}
            </h3>
            <p className='mt-1 text-sm text-muted-foreground'>
              {t('settings.appearance.background.description')}
            </p>
          </div>

          <div className='content-panel'>
            {/* 背景不透明度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.appearance.background.opacity.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.appearance.background.opacity.description')}
                </p>
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

            {/* 背景模糊度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.appearance.background.blurAmount.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.appearance.background.blurAmount.description')}
                </p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <div className='w-36'>
                  <Slider
                    value={[webSettings.ui.background.blurAmount]}
                    onValueChange={(value) => handleBlurAmountChange(value[0])}
                    min={0}
                    max={200}
                    step={1}
                    className='w-full'
                  />
                </div>
                <span className='w-12 text-sm text-muted-foreground'>
                  {webSettings.ui.background.blurAmount}px
                </span>
              </div>
            </div>

            {/* 背景图片控制 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.appearance.background.image.label')}
                </Label>
              </div>
              <div className='flex flex-shrink-0 gap-2'>
                <Button variant='outline' size='sm' onClick={handleSelectBackgroundImage}>
                  {t('settings.appearance.background.image.selectButton')}
                </Button>
                <Button
                  variant='outline'
                  size='sm'
                  onClick={handleRemoveBackgroundImage}
                  disabled={webSettings.ui?.background?.type === 'none'}
                >
                  {t('settings.appearance.background.image.removeButton')}
                </Button>
              </div>
            </div>
          </div>
        </div>

        {/* 主题设置 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>
              {t('settings.appearance.theme.title')}
            </h3>
            <p className='mt-1 text-sm text-muted-foreground'>
              {t('settings.appearance.theme.description')}
            </p>
          </div>

          <div className='content-panel'>
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.appearance.theme.mode.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.appearance.theme.mode.description')}
                </p>
              </div>
              <div className='flex flex-shrink-0'>
                <Select value={theme || 'system'} onValueChange={handleThemeChange}>
                  <SelectTrigger className='w-32'>
                    <SelectValue />
                  </SelectTrigger>
                  <SelectContent>
                    {themeOptions.map((option) => (
                      <SelectItem key={option.value} value={option.value}>
                        {option.label}
                      </SelectItem>
                    ))}
                  </SelectContent>
                </Select>
              </div>
            </div>

            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.appearance.appWindowTheme.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.appearance.appWindowTheme.description')}
                </p>
              </div>
              <div className='flex flex-shrink-0'>
                <Select
                  value={appSettings?.ui?.appWindowThemeMode || 'dark'}
                  onValueChange={handleAppWindowThemeChange}
                >
                  <SelectTrigger className='w-32'>
                    <SelectValue />
                  </SelectTrigger>
                  <SelectContent>
                    {appWindowThemeOptions.map((option) => (
                      <SelectItem key={option.value} value={option.value}>
                        {option.label}
                      </SelectItem>
                    ))}
                  </SelectContent>
                </Select>
              </div>
            </div>
          </div>
        </div>

        {/* 窗口外观参数 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>
              {t('settings.appearance.layout.title')}
            </h3>
            <p className='mt-1 text-sm text-muted-foreground'>
              {t('settings.appearance.layout.description')}
            </p>
          </div>

          <div className='content-panel'>
            {/* 基础项高度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.appearance.layout.baseItemHeight.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.baseItemHeight.description')}
                </p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings?.baseItemHeight || ''}
                  onChange={(e) => handleLayoutChange('baseItemHeight', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.unit')}
                </span>
              </div>
            </div>

            {/* 标题高度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.appearance.layout.baseTitleHeight.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.baseTitleHeight.description')}
                </p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings?.baseTitleHeight || ''}
                  onChange={(e) => handleLayoutChange('baseTitleHeight', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.unit')}
                </span>
              </div>
            </div>

            {/* 分隔线高度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.appearance.layout.baseSeparatorHeight.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.baseSeparatorHeight.description')}
                </p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings?.baseSeparatorHeight || ''}
                  onChange={(e) => handleLayoutChange('baseSeparatorHeight', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.unit')}
                </span>
              </div>
            </div>

            {/* 基础字体大小 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.appearance.layout.baseFontSize.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.baseFontSize.description')}
                </p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings?.baseFontSize || ''}
                  onChange={(e) => handleLayoutChange('baseFontSize', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.unit')}
                </span>
              </div>
            </div>

            {/* 文本内边距 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.appearance.layout.baseTextPadding.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.baseTextPadding.description')}
                </p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings?.baseTextPadding || ''}
                  onChange={(e) => handleLayoutChange('baseTextPadding', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.unit')}
                </span>
              </div>
            </div>

            {/* 指示器宽度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.appearance.layout.baseIndicatorWidth.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.baseIndicatorWidth.description')}
                </p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings?.baseIndicatorWidth || ''}
                  onChange={(e) => handleLayoutChange('baseIndicatorWidth', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.unit')}
                </span>
              </div>
            </div>

            {/* 比例指示器宽度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.appearance.layout.baseRatioIndicatorWidth.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.baseRatioIndicatorWidth.description')}
                </p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings?.baseRatioIndicatorWidth || ''}
                  onChange={(e) => handleLayoutChange('baseRatioIndicatorWidth', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.unit')}
                </span>
              </div>
            </div>

            {/* 比例列宽度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.appearance.layout.baseRatioColumnWidth.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.baseRatioColumnWidth.description')}
                </p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings?.baseRatioColumnWidth || ''}
                  onChange={(e) => handleLayoutChange('baseRatioColumnWidth', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.unit')}
                </span>
              </div>
            </div>

            {/* 分辨率列宽度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.appearance.layout.baseResolutionColumnWidth.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.baseResolutionColumnWidth.description')}
                </p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings?.baseResolutionColumnWidth || ''}
                  onChange={(e) => handleLayoutChange('baseResolutionColumnWidth', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.unit')}
                </span>
              </div>
            </div>

            {/* 设置列宽度 */}
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.appearance.layout.baseSettingsColumnWidth.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.baseSettingsColumnWidth.description')}
                </p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  type='number'
                  value={layoutSettings?.baseSettingsColumnWidth || ''}
                  onChange={(e) => handleLayoutChange('baseSettingsColumnWidth', e.target.value)}
                  onKeyDown={handleKeyDown}
                  className='w-24'
                  min='0'
                />
                <span className='text-sm text-muted-foreground'>
                  {t('settings.appearance.layout.unit')}
                </span>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}
