import { useState, useEffect } from 'react'
import { toast } from 'sonner'
import { useFunctionActions, useFunctionStore } from '../store/function-store'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { call } from '@/lib/webview-rpc'
import { Switch } from '@/components/ui/switch'

export function FunctionContent() {
  const { appSettings, error, isInitialized, clearError } = useFunctionStore()
  const {
    updateWindowTitle,
    updateScreenshotDir,
    updateTaskbarLowerOnResize,
    updateLetterboxEnabled,
  } = useFunctionActions()

  const [inputTitle, setInputTitle] = useState('')
  const [isUpdatingTitle, setIsUpdatingTitle] = useState(false)
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

  const handleSelectDir = async () => {
    setIsSelectingDir(true)
    try {
      const result = await call<{ path: string }>('screenshot.folder.select', undefined, 45000)
      await updateScreenshotDir(result.path)
      toast.success('截图目录已更新')
    } catch (error) {
      console.error('Failed to select screenshot directory:', error)
      toast.error('选择截图目录失败')
    } finally {
      setIsSelectingDir(false)
    }
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
    <div className='p-6'>
      {/* 页面标题 */}
      <div className='mb-6'>
        <h1 className='text-2xl font-bold text-foreground'>功能设置</h1>
        <p className='mt-1 text-muted-foreground'>管理应用程序的各项功能设置</p>
      </div>

      <div className='space-y-8'>
        {/* 窗口控制 */}
        <div className='space-y-4'>
          <div className='pb-2'>
            <h3 className='text-lg font-semibold text-foreground'>窗口控制</h3>
            <p className='mt-1 text-sm text-muted-foreground'>自定义应用程序窗口的控制设置</p>
          </div>

          <div className='space-y-4 border-l-2 border-border pl-4'>
            <div className='flex items-center justify-between py-4'>
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
                  disabled={isUpdatingTitle}
                />
                <Button
                  onClick={handleUpdateTitle}
                  disabled={isUpdatingTitle || inputTitle.trim() === ''}
                  size='sm'
                >
                  {isUpdatingTitle ? '更新中...' : '更新'}
                </Button>
              </div>
            </div>

            <div className='flex items-center justify-between py-4'>
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
          <div className='pb-2'>
            <h3 className='text-lg font-semibold text-foreground'>截图设置</h3>
            <p className='mt-1 text-sm text-muted-foreground'>自定义截图相关的设置</p>
          </div>

          <div className='space-y-4 border-l-2 border-border pl-4'>
            <div className='flex items-center justify-between py-4'>
              <div className='flex-1 pr-4'>
                <Label className='text-sm font-medium text-foreground'>截图目录</Label>
                <p className='mt-1 text-sm text-muted-foreground'>设置截图保存的目录路径</p>
              </div>
              <div className='flex flex-shrink-0 items-center gap-2'>
                <Input
                  value={screenshotDir}
                  readOnly
                  placeholder='未设置截图目录'
                  className='w-48'
                />
                <Button onClick={handleSelectDir} disabled={isSelectingDir} size='sm'>
                  {isSelectingDir ? '选择中...' : '选择目录'}
                </Button>
              </div>
            </div>
          </div>
        </div>

        {/* 黑边模式 */}
        <div className='space-y-4'>
          <div className='pb-2'>
            <h3 className='text-lg font-semibold text-foreground'>黑边模式</h3>
            <p className='mt-1 text-sm text-muted-foreground'>启用后在目标窗口周围添加黑边遮罩</p>
          </div>
          <div className='space-y-4 border-l-2 border-border pl-4'>
            <div className='flex items-center justify-between py-4'>
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
