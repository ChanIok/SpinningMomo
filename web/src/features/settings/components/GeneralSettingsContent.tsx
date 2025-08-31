import { toast } from 'sonner'
import { useSettingsStore } from '@/lib/settings'
import { useGeneralActions } from '@/features/settings/hooks/useGeneralActions'
import { Button } from '@/components/ui/button'
import { Label } from '@/components/ui/label'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { HotkeyRecorder } from './HotkeyRecorder'
import { ResetSettingsDialog } from './ResetSettingsDialog'
import { useTranslation } from '@/lib/i18n'

export function GeneralSettingsContent() {
  const { t } = useTranslation()
  const { appSettings, error, isInitialized, clearError } = useSettingsStore()
  const {
    updateLanguage,
    updateLoggerLevel,
    updateToggleVisibilityHotkey,
    updateScreenshotHotkey,
    resetGeneralSettings,
  } = useGeneralActions()

  const handleResetSettings = async () => {
    await resetGeneralSettings()
    toast.success(t('settings.reset.success'))
  }

  // 显示加载状态（仅在未初始化时）
  if (!isInitialized) {
    return (
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <div className='mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary'></div>
          <p className='mt-2 text-sm text-muted-foreground'>{t('settings.loading')}</p>
        </div>
      </div>
    )
  }

  // 显示错误状态
  if (error) {
    return (
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <p className='text-sm text-muted-foreground'>{t('settings.error.title')}</p>
          <p className='mt-1 text-sm text-red-500'>{error}</p>
          <Button variant='outline' size='sm' onClick={clearError} className='mt-2'>
            {t('settings.error.retry')}
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
          <h1 className='text-2xl font-bold text-foreground'>{t('settings.general.title')}</h1>
          <p className='mt-1 text-muted-foreground'>{t('settings.general.description')}</p>
        </div>

        <ResetSettingsDialog
          title={t('settings.reset.title')}
          description={t('settings.reset.description')}
          onReset={handleResetSettings}
        />
      </div>

      <div className='space-y-8'>
        {/* 语言设置 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>
              {t('settings.general.language.title')}
            </h3>
            <p className='mt-1 text-sm text-muted-foreground'>
              {t('settings.general.language.description')}
            </p>
          </div>

          <div className='content-panel'>
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.general.language.displayLanguage')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.general.language.displayLanguageDescription')}
                </p>
              </div>
              <div className='w-48 flex-shrink-0'>
                <Select
                  value={appSettings.app.language.current}
                  onValueChange={(value) => updateLanguage(value)}
                >
                  <SelectTrigger>
                    <SelectValue placeholder={t('settings.general.language.displayLanguage')} />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value='zh-CN'>{t('common.languageZhCn')}</SelectItem>
                    <SelectItem value='en-US'>{t('common.languageEnUs')}</SelectItem>
                  </SelectContent>
                </Select>
              </div>
            </div>
          </div>
        </div>

        {/* 日志设置 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>
              {t('settings.general.logger.title')}
            </h3>
            <p className='mt-1 text-sm text-muted-foreground'>
              {t('settings.general.logger.description')}
            </p>
          </div>

          <div className='content-panel'>
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.general.logger.level')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.general.logger.levelDescription')}
                </p>
              </div>
              <div className='w-48 flex-shrink-0'>
                <Select
                  value={appSettings.app.logger.level}
                  onValueChange={(value) => updateLoggerLevel(value)}
                >
                  <SelectTrigger>
                    <SelectValue placeholder={t('settings.general.logger.level')} />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value='DEBUG'>DEBUG</SelectItem>
                    <SelectItem value='INFO'>INFO</SelectItem>
                    <SelectItem value='ERROR'>ERROR</SelectItem>
                  </SelectContent>
                </Select>
              </div>
            </div>
          </div>
        </div>

        {/* 快捷键设置 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>
              {t('settings.general.hotkey.title')}
            </h3>

            <span className='text-sm text-muted-foreground'>
              {t('settings.general.hotkey.description')}
            </span>
          </div>

          <div className='content-panel'>
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.general.hotkey.toggleVisibility')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.general.hotkey.toggleVisibilityDescription')}
                </p>
              </div>
              <div className='w-48 flex-shrink-0'>
                <HotkeyRecorder
                  value={{
                    modifiers: appSettings.app.hotkey.toggleVisibility.modifiers,
                    key: appSettings.app.hotkey.toggleVisibility.key,
                  }}
                  onChange={(newHotkey) =>
                    updateToggleVisibilityHotkey(newHotkey.modifiers, newHotkey.key)
                  }
                />
              </div>
            </div>

            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>
                  {t('settings.general.hotkey.screenshot')}
                </Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('settings.general.hotkey.screenshotDescription')}
                </p>
              </div>
              <div className='w-48 flex-shrink-0'>
                <HotkeyRecorder
                  value={{
                    modifiers: appSettings.app.hotkey.screenshot.modifiers,
                    key: appSettings.app.hotkey.screenshot.key,
                  }}
                  onChange={(newHotkey) =>
                    updateScreenshotHotkey(newHotkey.modifiers, newHotkey.key)
                  }
                />
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}
