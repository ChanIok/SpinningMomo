import { useState } from 'react'
import { toast } from 'sonner'
import { useSettingsStore } from '@/lib/settings'
import { useFunctionActions } from '@/features/settings/hooks/useFunctionActions'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { call } from '@/lib/rpc'
import { Switch } from '@/components/ui/switch'
import { getCurrentEnvironment } from '@/lib/environment'
import { ResetSettingsDialog } from './ResetSettingsDialog'
import { useTranslation } from '@/lib/i18n'

export function FunctionContent() {
  const { t } = useTranslation()
  const { appSettings, error, isInitialized, clearError } = useSettingsStore()
  const {
    updateWindowTitle,
    updateScreenshotDir,
    updateTaskbarLowerOnResize,
    updateLetterboxEnabled,
    resetFunctionSettings,
  } = useFunctionActions()

  const [isSelectingDir, setIsSelectingDir] = useState(false)

  const inputTitle = appSettings?.window?.targetTitle || ''
  const screenshotDir = appSettings?.features?.screenshot?.screenshotDirPath || ''

  const handleTitleChange = async (value: string) => {
    if (value.trim() === '') {
      toast.error(t('settings.function.windowControl.windowTitle.emptyError'))
      return
    }

    try {
      await updateWindowTitle(value.trim())
      toast.success(t('settings.function.windowControl.windowTitle.updateSuccess'))
    } catch (error) {
      console.error('Failed to update window title:', error)
      toast.error(t('settings.function.windowControl.windowTitle.updateFailed'))
    }
  }

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') {
      handleTitleChange(inputTitle)
    }
  }

  const handleSelectDir = async () => {
    setIsSelectingDir(true)
    try {
      // 根据当前环境选择父窗口模式
      const environment = getCurrentEnvironment()
      const parentWindowMode = environment === 'webview' ? 1 : 2 // webview: 1, web: 2

      const result = await call<{ path: string }>(
        'dialog.openDirectory',
        {
          title: t('settings.function.screenshot.directory.dialogTitle'),
          parentWindowMode,
        },
        0
      ) // 永不超时
      await updateScreenshotDir(result.path)
      toast.success(t('settings.function.screenshot.directory.selectSuccess'))
    } catch (error) {
      console.error('Failed to select screenshot directory:', error)
      toast.error(t('settings.function.screenshot.directory.selectFailed'))
    } finally {
      setIsSelectingDir(false)
    }
  }

  const handleResetSettings = async () => {
    await resetFunctionSettings()
    toast.success(t('settings.function.reset.success'))
  }

  // 显示加载状态（仅在未初始化时）
  if (!isInitialized) {
    return (
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <div className='mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary'></div>
          <p className='mt-2 text-sm text-muted-foreground'>{t('settings.function.loading')}</p>
        </div>
      </div>
    )
  }

  // 显示错误状态
  if (error) {
    return (
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <p className='text-sm text-muted-foreground'>{t('settings.function.error.title')}</p>
          <p className='mt-1 text-sm text-red-500'>{error}</p>
          <Button variant='outline' size='sm' onClick={clearError} className='mt-2'>
            {t('settings.function.error.retry')}
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
          <h1 className='text-2xl font-bold text-foreground'>{t('settings.function.title')}</h1>
          <p className='mt-1 text-muted-foreground'>{t('settings.function.description')}</p>
        </div>

        <ResetSettingsDialog
          title={t('settings.function.reset.title')}
          description={t('settings.function.reset.description')}
          onReset={handleResetSettings}
        />
      </div>

      <div className='space-y-8'>
        {/* 窗口控制 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>
              {t('settings.function.windowControl.title')}
            </h3>
            <p className='mt-1 text-sm text-muted-foreground'>
              {t('settings.function.windowControl.description')}
            </p>
          </div>

          <div className='content-panel'>
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.function.windowControl.windowTitle.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.function.windowControl.windowTitle.description')}
                </p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  value={inputTitle}
                  onChange={(e) => handleTitleChange(e.target.value)}
                  onKeyDown={handleKeyDown}
                  placeholder={t('settings.function.windowControl.windowTitle.placeholder')}
                  className='w-48'
                />
                <Button
                  onClick={() => handleTitleChange(inputTitle)}
                  disabled={inputTitle.trim() === ''}
                  size='sm'
                >
                  {t('settings.function.windowControl.windowTitle.update')}
                </Button>
              </div>
            </div>

            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.function.windowControl.taskbarLowerOnResize.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.function.windowControl.taskbarLowerOnResize.description')}
                </p>
              </div>
              <div className='flex-shrink-0'>
                <Switch
                  checked={appSettings?.window?.taskbar?.lowerOnResize ?? true}
                  onCheckedChange={updateTaskbarLowerOnResize}
                />
              </div>
            </div>
          </div>
        </div>

        {/* 截图设置 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>
              {t('settings.function.screenshot.title')}
            </h3>
            <p className='mt-1 text-sm text-muted-foreground'>
              {t('settings.function.screenshot.description')}
            </p>
          </div>

          <div className='content-panel'>
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.function.screenshot.directory.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.function.screenshot.directory.description')}
                </p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  value={screenshotDir}
                  readOnly
                  placeholder={t('settings.function.screenshot.directory.placeholder')}
                  className='w-48'
                />
                <Button onClick={handleSelectDir} disabled={isSelectingDir} size='sm'>
                  {isSelectingDir
                    ? t('settings.function.screenshot.directory.selecting')
                    : t('settings.function.screenshot.directory.selectButton')}
                </Button>
              </div>
            </div>
          </div>
        </div>

        {/* 黑边模式 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>
              {t('settings.function.letterbox.title')}
            </h3>
            <p className='mt-1 text-sm text-muted-foreground'>
              {t('settings.function.letterbox.description')}
            </p>
          </div>
          <div className='content-panel'>
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.function.letterbox.enabled.label')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.function.letterbox.enabled.description')}
                </p>
              </div>
              <div className='flex-shrink-0'>
                <Switch
                  checked={appSettings?.features?.letterbox?.enabled ?? false}
                  onCheckedChange={updateLetterboxEnabled}
                />
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}
