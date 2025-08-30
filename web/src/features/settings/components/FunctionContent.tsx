import { useState, useEffect } from 'react'
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

export function FunctionContent() {
  const { appSettings, error, isInitialized, clearError } = useSettingsStore()
  const {
    updateWindowTitle,
    updateScreenshotDir,
    updateTaskbarLowerOnResize,
    updateLetterboxEnabled,
    resetFunctionSettings,
  } = useFunctionActions()

  const [inputTitle, setInputTitle] = useState('')
  const [screenshotDir, setScreenshotDir] = useState('')
  const [isSelectingDir, setIsSelectingDir] = useState(false)

  // 同步store中的标题到输入框（适配新的嵌套结构）
  useEffect(() => {
    setInputTitle(appSettings?.window?.targetTitle || '')
  }, [appSettings?.window?.targetTitle])

  // 同步store中的截图目录到状态
  useEffect(() => {
    setScreenshotDir(appSettings?.features?.screenshot?.screenshotDirPath || '')
  }, [appSettings?.features?.screenshot?.screenshotDirPath])

  const handleUpdateTitle = async () => {
    if (inputTitle.trim() === '') {
      toast.error('窗口标题不能为空')
      return
    }

    try {
      await updateWindowTitle(inputTitle.trim())
      toast.success('窗口标题已更新')
    } catch (error) {
      console.error('Failed to update window title:', error)
      toast.error('更新窗口标题失败')
    }
  }

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') {
      handleUpdateTitle()
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
          title: '选择截图目录',
          parentWindowMode,
        },
        0
      ) // 永不超时
      await updateScreenshotDir(result.path)
      toast.success('截图目录已更新')
    } catch (error) {
      console.error('Failed to select screenshot directory:', error)
      toast.error('选择截图目录失败')
    } finally {
      setIsSelectingDir(false)
    }
  }

  const handleResetSettings = async () => {
    await resetFunctionSettings()
    toast.success('功能设置已重置为默认值')
  }

  // 显示加载状态（仅在未初始化时）
  if (!isInitialized) {
    return (
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <div className='mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary'></div>
          <p className='mt-2 text-sm text-muted-foreground'>加载功能设置中...</p>
        </div>
      </div>
    )
  }

  // 显示错误状态
  if (error) {
    return (
      <div className='flex items-center justify-center p-6'>
        <div className='text-center'>
          <p className='text-sm text-muted-foreground'>无法加载功能设置</p>
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
          <h1 className='text-2xl font-bold text-foreground'>功能设置</h1>
          <p className='mt-1 text-muted-foreground'>管理应用程序的各项功能设置</p>
        </div>

        <ResetSettingsDialog
          title='重置功能设置'
          description=' 此操作将重置当前页面设置为默认值。'
          onReset={handleResetSettings}
        />
      </div>

      <div className='space-y-8'>
        {/* 窗口控制 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>窗口控制</h3>
            <p className='mt-1 text-sm text-muted-foreground'>自定义应用程序窗口的控制设置</p>
          </div>

          <div className='space-y-4 rounded-md border border-border bg-card p-4'>
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>窗口标题</Label>
                <p className='mt-1 text-sm text-muted-foreground'>设置目标窗口的标题栏文本</p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  value={inputTitle}
                  onChange={(e) => setInputTitle(e.target.value)}
                  onKeyDown={handleKeyDown}
                  placeholder='输入窗口标题...'
                  className='w-48'
                />
                <Button onClick={handleUpdateTitle} disabled={inputTitle.trim() === ''} size='sm'>
                  更新
                </Button>
              </div>
            </div>

            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>调整时置底任务栏</Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  窗口调整大小时将系统任务栏置于底层，避免遮挡目标窗口
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
            <h3 className='text-lg font-semibold text-foreground'>截图设置</h3>
            <p className='mt-1 text-sm text-muted-foreground'>自定义截图相关的设置</p>
          </div>

          <div className='space-y-4 rounded-md border border-border bg-card p-4'>
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>截图目录</Label>
                <p className='mt-1 text-sm text-muted-foreground'>设置截图保存的目录路径</p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input value={screenshotDir} readOnly placeholder='默认路径' className='w-48' />
                <Button onClick={handleSelectDir} disabled={isSelectingDir} size='sm'>
                  {isSelectingDir ? '选择中...' : '选择目录'}
                </Button>
              </div>
            </div>
          </div>
        </div>

        {/* 黑边模式 */}
        <div className='space-y-4'>
          <div>
            <h3 className='text-lg font-semibold text-foreground'>黑边模式</h3>
            <p className='mt-1 text-sm text-muted-foreground'>启用后在目标窗口周围添加黑边遮罩</p>
          </div>
          <div className='space-y-4 rounded-md border border-border bg-card p-4'>
            <div className='flex items-center justify-between py-2'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>是否启用黑边模式</Label>
                <p className='mt-1 text-sm text-muted-foreground'>
                  为非屏幕原生比例的窗口提供沉浸式体验
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
