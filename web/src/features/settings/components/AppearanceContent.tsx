import { useState, useEffect, useMemo } from 'react'
import { toast } from 'sonner'
import { useSettingsStore } from '@/lib/settings'
import { useAppearanceActions } from '@/features/settings/hooks/useAppearanceActions'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import type { AppWindowLayout } from '@/lib/settings/settingsTypes'

export function AppearanceContent() {
  const { appSettings, error, isInitialized, clearError } = useSettingsStore()
  const { updateAppWindowLayout } = useAppearanceActions()

  // 保存原始设置用于重置
  const [originalLayoutSettings, setOriginalLayoutSettings] = useState<AppWindowLayout>({
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

  const [isUpdating, setIsUpdating] = useState(false)

  // 检查是否有更改
  const hasChanges = useMemo(() => {
    return JSON.stringify(layoutSettings) !== JSON.stringify(originalLayoutSettings)
  }, [layoutSettings, originalLayoutSettings])

  // 同步store中的布局设置到本地状态
  useEffect(() => {
    if (appSettings?.ui?.appWindowLayout) {
      setLayoutSettings(appSettings.ui.appWindowLayout)
      setOriginalLayoutSettings(appSettings.ui.appWindowLayout)
    }
  }, [appSettings?.ui?.appWindowLayout])

  const handleUpdateLayout = async () => {
    setIsUpdating(true)
    try {
      await updateAppWindowLayout(layoutSettings)
      toast.success('外观设置已应用')
      // 更新原始设置为当前设置
      setOriginalLayoutSettings({ ...layoutSettings })
    } catch (error) {
      console.error('Failed to update layout settings:', error)
      toast.error('应用外观设置失败')
    } finally {
      setIsUpdating(false)
    }
  }

  const handleReset = () => {
    setLayoutSettings({ ...originalLayoutSettings })
  }

  const handleInputChange = (field: keyof AppWindowLayout, value: string) => {
    const numValue = parseInt(value, 10)
    if (!isNaN(numValue) && numValue >= 0) {
      setLayoutSettings((prev) => ({
        ...prev,
        [field]: numValue,
      }))
    }
  }

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') {
      handleUpdateLayout()
    }
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
  if (error) {
    return (
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <p className='text-sm text-muted-foreground'>无法加载外观设置</p>
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
        <h1 className='text-2xl font-bold text-foreground'>外观设置</h1>
        <p className='mt-1 text-muted-foreground'>自定义应用程序窗口的外观参数</p>
      </div>

      {/* 滚动区域 */}
      <div className='flex-1 overflow-y-auto pb-20'>
        <div className='space-y-8'>
          {/* 布局参数设置 */}
          <div className='space-y-6'>
            <div className='pb-2'>
              <h3 className='text-lg font-semibold text-foreground'>窗口外观参数</h3>
              <p className='mt-1 text-sm text-muted-foreground'>
                调整应用程序窗口中各元素的尺寸和间距
              </p>
            </div>

            <div className='space-y-4 border-l-2 border-border pl-4'>
              {/* 基础项高度 */}
              <div className='flex items-center justify-between py-4'>
                <div className='flex-1 pr-4'>
                  <Label className='text-sm font-medium text-foreground'>基础项高度</Label>
                  <p className='mt-1 text-sm text-muted-foreground'>设置菜单项的基础高度（像素）</p>
                </div>
                <div className='flex flex-shrink-0 items-center gap-2'>
                  <Input
                    type='number'
                    value={layoutSettings.baseItemHeight}
                    onChange={(e) => handleInputChange('baseItemHeight', e.target.value)}
                    onKeyDown={handleKeyDown}
                    className='w-24'
                    min='0'
                  />
                  <span className='text-sm text-muted-foreground'>px</span>
                </div>
              </div>

              {/* 标题高度 */}
              <div className='flex items-center justify-between py-4'>
                <div className='flex-1 pr-4'>
                  <Label className='text-sm font-medium text-foreground'>标题高度</Label>
                  <p className='mt-1 text-sm text-muted-foreground'>设置标题区域的高度（像素）</p>
                </div>
                <div className='flex flex-shrink-0 items-center gap-2'>
                  <Input
                    type='number'
                    value={layoutSettings.baseTitleHeight}
                    onChange={(e) => handleInputChange('baseTitleHeight', e.target.value)}
                    onKeyDown={handleKeyDown}
                    className='w-24'
                    min='0'
                  />
                  <span className='text-sm text-muted-foreground'>px</span>
                </div>
              </div>

              {/* 分隔线高度 */}
              <div className='flex items-center justify-between py-4'>
                <div className='flex-1 pr-4'>
                  <Label className='text-sm font-medium text-foreground'>分隔线高度</Label>
                  <p className='mt-1 text-sm text-muted-foreground'>设置分隔线的高度（像素）</p>
                </div>
                <div className='flex flex-shrink-0 items-center gap-2'>
                  <Input
                    type='number'
                    value={layoutSettings.baseSeparatorHeight}
                    onChange={(e) => handleInputChange('baseSeparatorHeight', e.target.value)}
                    onKeyDown={handleKeyDown}
                    className='w-24'
                    min='0'
                  />
                  <span className='text-sm text-muted-foreground'>px</span>
                </div>
              </div>

              {/* 基础字体大小 */}
              <div className='flex items-center justify-between py-4'>
                <div className='flex-1 pr-4'>
                  <Label className='text-sm font-medium text-foreground'>基础字体大小</Label>
                  <p className='mt-1 text-sm text-muted-foreground'>设置基础字体大小（像素）</p>
                </div>
                <div className='flex flex-shrink-0 items-center gap-2'>
                  <Input
                    type='number'
                    value={layoutSettings.baseFontSize}
                    onChange={(e) => handleInputChange('baseFontSize', e.target.value)}
                    onKeyDown={handleKeyDown}
                    className='w-24'
                    min='0'
                  />
                  <span className='text-sm text-muted-foreground'>px</span>
                </div>
              </div>

              {/* 文本内边距 */}
              <div className='flex items-center justify-between py-4'>
                <div className='flex-1 pr-4'>
                  <Label className='text-sm font-medium text-foreground'>文本内边距</Label>
                  <p className='mt-1 text-sm text-muted-foreground'>设置文本的内边距（像素）</p>
                </div>
                <div className='flex flex-shrink-0 items-center gap-2'>
                  <Input
                    type='number'
                    value={layoutSettings.baseTextPadding}
                    onChange={(e) => handleInputChange('baseTextPadding', e.target.value)}
                    onKeyDown={handleKeyDown}
                    className='w-24'
                    min='0'
                  />
                  <span className='text-sm text-muted-foreground'>px</span>
                </div>
              </div>

              {/* 指示器宽度 */}
              <div className='flex items-center justify-between py-4'>
                <div className='flex-1 pr-4'>
                  <Label className='text-sm font-medium text-foreground'>指示器宽度</Label>
                  <p className='mt-1 text-sm text-muted-foreground'>设置指示器的宽度（像素）</p>
                </div>
                <div className='flex flex-shrink-0 items-center gap-2'>
                  <Input
                    type='number'
                    value={layoutSettings.baseIndicatorWidth}
                    onChange={(e) => handleInputChange('baseIndicatorWidth', e.target.value)}
                    onKeyDown={handleKeyDown}
                    className='w-24'
                    min='0'
                  />
                  <span className='text-sm text-muted-foreground'>px</span>
                </div>
              </div>

              {/* 比例指示器宽度 */}
              <div className='flex items-center justify-between py-4'>
                <div className='flex-1 pr-4'>
                  <Label className='text-sm font-medium text-foreground'>比例指示器宽度</Label>
                  <p className='mt-1 text-sm text-muted-foreground'>设置比例指示器的宽度（像素）</p>
                </div>
                <div className='flex flex-shrink-0 items-center gap-2'>
                  <Input
                    type='number'
                    value={layoutSettings.baseRatioIndicatorWidth}
                    onChange={(e) => handleInputChange('baseRatioIndicatorWidth', e.target.value)}
                    onKeyDown={handleKeyDown}
                    className='w-24'
                    min='0'
                  />
                  <span className='text-sm text-muted-foreground'>px</span>
                </div>
              </div>

              {/* 比例列宽度 */}
              <div className='flex items-center justify-between py-4'>
                <div className='flex-1 pr-4'>
                  <Label className='text-sm font-medium text-foreground'>比例列宽度</Label>
                  <p className='mt-1 text-sm text-muted-foreground'>设置比例列的宽度（像素）</p>
                </div>
                <div className='flex flex-shrink-0 items-center gap-2'>
                  <Input
                    type='number'
                    value={layoutSettings.baseRatioColumnWidth}
                    onChange={(e) => handleInputChange('baseRatioColumnWidth', e.target.value)}
                    onKeyDown={handleKeyDown}
                    className='w-24'
                    min='0'
                  />
                  <span className='text-sm text-muted-foreground'>px</span>
                </div>
              </div>

              {/* 分辨率列宽度 */}
              <div className='flex items-center justify-between py-4'>
                <div className='flex-1 pr-4'>
                  <Label className='text-sm font-medium text-foreground'>分辨率列宽度</Label>
                  <p className='mt-1 text-sm text-muted-foreground'>设置分辨率列的宽度（像素）</p>
                </div>
                <div className='flex flex-shrink-0 items-center gap-2'>
                  <Input
                    type='number'
                    value={layoutSettings.baseResolutionColumnWidth}
                    onChange={(e) => handleInputChange('baseResolutionColumnWidth', e.target.value)}
                    onKeyDown={handleKeyDown}
                    className='w-24'
                    min='0'
                  />
                  <span className='text-sm text-muted-foreground'>px</span>
                </div>
              </div>

              {/* 设置列宽度 */}
              <div className='flex items-center justify-between py-4'>
                <div className='flex-1 pr-4'>
                  <Label className='text-sm font-medium text-foreground'>设置列宽度</Label>
                  <p className='mt-1 text-sm text-muted-foreground'>设置设置列的宽度（像素）</p>
                </div>
                <div className='flex flex-shrink-0 items-center gap-2'>
                  <Input
                    type='number'
                    value={layoutSettings.baseSettingsColumnWidth}
                    onChange={(e) => handleInputChange('baseSettingsColumnWidth', e.target.value)}
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

      {/* 固定在底部的操作按钮 */}
      {(hasChanges || isUpdating) && (
        <div className='fixed right-0 bottom-0 left-0 border-t border-border bg-background p-4 shadow-lg'>
          <div className='flex justify-end gap-2'>
            <Button variant='outline' onClick={handleReset} disabled={isUpdating}>
              取消
            </Button>
            <Button onClick={handleUpdateLayout} disabled={isUpdating}>
              {isUpdating ? '应用中...' : '应用'}
            </Button>
          </div>
        </div>
      )}
    </div>
  )
}
