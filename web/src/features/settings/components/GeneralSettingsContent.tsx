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

export function GeneralSettingsContent() {
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
    toast.success('通用设置已重置为默认值')
  }

  // 显示加载状态（仅在未初始化时）
  if (!isInitialized) {
    return (
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <div className='mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary'></div>
          <p className='mt-2 text-sm text-muted-foreground'>加载通用设置中...</p>
        </div>
      </div>
    )
  }

  // 显示错误状态
  if (error) {
    return (
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <p className='text-sm text-muted-foreground'>无法加载通用设置</p>
          <p className='mt-1 text-sm text-red-500'>{error}</p>
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
          <h1 className='text-2xl font-bold text-foreground'>通用设置</h1>
          <p className='mt-1 text-muted-foreground'>管理应用程序的核心设置</p>
        </div>

        <ResetSettingsDialog
          title='重置通用设置'
          description='此操作将重置当前页面设置为默认值。'
          onReset={handleResetSettings}
        />
      </div>

      <div className='space-y-8'>
        {/* 语言设置 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>语言设置</h3>
            <p className='mt-1 text-sm text-muted-foreground'>选择应用程序的显示语言</p>
          </div>

          <div className='space-y-4 rounded-md border border-border bg-card p-4'>
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>显示语言</Label>
                <p className='mt-1 text-sm text-muted-foreground'>更改应用程序界面的语言</p>
              </div>
              <div className='w-48 flex-shrink-0'>
                <Select
                  value={appSettings.app.language.current}
                  onValueChange={(value) => updateLanguage(value)}
                >
                  <SelectTrigger>
                    <SelectValue placeholder='选择语言' />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value='zh-CN'>简体中文</SelectItem>
                    <SelectItem value='en-US'>English</SelectItem>
                  </SelectContent>
                </Select>
              </div>
            </div>
          </div>
        </div>

        {/* 日志设置 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>日志设置</h3>
            <p className='mt-1 text-sm text-muted-foreground'>配置应用程序的日志记录级别</p>
          </div>

          <div className='space-y-4 rounded-md border border-border bg-card p-4'>
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>日志级别</Label>
                <p className='mt-1 text-sm text-muted-foreground'>控制记录的日志详细程度</p>
              </div>
              <div className='w-48 flex-shrink-0'>
                <Select
                  value={appSettings.app.logger.level}
                  onValueChange={(value) => updateLoggerLevel(value)}
                >
                  <SelectTrigger>
                    <SelectValue placeholder='选择日志级别' />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value='DEBUG'>调试 (DEBUG)</SelectItem>
                    <SelectItem value='INFO'>信息 (INFO)</SelectItem>
                    <SelectItem value='ERROR'>错误 (ERROR)</SelectItem>
                  </SelectContent>
                </Select>
              </div>
            </div>
          </div>
        </div>

        {/* 快捷键设置 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>快捷键设置</h3>

            <span className='text-sm text-muted-foreground'>自定义应用程序的快捷键</span>
          </div>

          <div className='space-y-4 rounded-md border border-border bg-card p-4'>
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>浮窗显示/隐藏快捷键</Label>
                <p className='mt-1 text-sm text-muted-foreground'>设置显示或隐藏浮窗的快捷键</p>
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
                <Label className='text-sm font-medium text-foreground'>截图快捷键</Label>
                <p className='mt-1 text-sm text-muted-foreground'>设置触发截图功能的快捷键</p>
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
