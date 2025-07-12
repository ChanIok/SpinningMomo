import { useState, useEffect } from 'react'
import { toast } from 'sonner'
import { useMenuStore } from '../store/menu-store'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'

export function MenuContent() {
  const {
    windowTitle,
    isLoading,
    error,
    isInitialized,
    updateWindowTitle,
    clearError
  } = useMenuStore()

  const [inputTitle, setInputTitle] = useState('')

  // 同步store中的标题到输入框
  useEffect(() => {
    setInputTitle(windowTitle)
  }, [windowTitle])

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

  // 显示加载状态（仅在未初始化时）
  if (!isInitialized && isLoading) {
    return (
      <div className="p-6 flex items-center justify-center">
        <div className="text-center">
          <div className="h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary mx-auto"></div>
          <p className="text-sm text-muted-foreground mt-2">加载菜单中...</p>
        </div>
      </div>
    )
  }

  // 显示错误状态
  if (error) {
    return (
      <div className="p-6 flex items-center justify-center">
        <div className="text-center">
          <p className="text-sm text-muted-foreground">无法加载菜单</p>
          <p className="text-sm text-red-500 mt-1">{error}</p>
          <Button 
            variant="outline" 
            size="sm" 
            onClick={clearError}
            className="mt-2"
          >
            重试
          </Button>
        </div>
      </div>
    )
  }

  return (
    <div className="p-6 max-w-4xl">
      {/* 页面标题 */}
      <div className="mb-6">
        <h1 className="text-2xl font-bold text-foreground">菜单</h1>
        <p className="text-muted-foreground mt-1">
          管理应用程序的各种功能和设置
        </p>
      </div>

      <div className="space-y-8">
        {/* 窗口控制 */}
        <div className="space-y-4">
          <div className="pb-2">
            <h3 className="text-lg font-semibold text-foreground">窗口控制</h3>
            <p className="text-sm text-muted-foreground mt-1">
              自定义应用程序窗口的显示设置
            </p>
          </div>
          
          <div className="border-l-2 border-border pl-4 space-y-4">
            <div className="flex items-center justify-between py-4">
              <div className="flex-1 pr-4">
                <Label className="text-sm font-medium text-foreground">
                  窗口标题
                </Label>
                <p className="text-sm text-muted-foreground mt-1">
                  设置应用程序窗口的标题栏文本
                </p>
              </div>
              <div className="flex-shrink-0 flex items-center gap-2">
                <Input
                  value={inputTitle}
                  onChange={(e) => setInputTitle(e.target.value)}
                  onKeyDown={handleKeyDown}
                  placeholder="输入窗口标题..."
                  className="w-48"
                  disabled={isLoading}
                />
                <Button
                  onClick={handleUpdateTitle}
                  disabled={isLoading || inputTitle.trim() === ''}
                  size="sm"
                >
                  {isLoading ? '更新中...' : '更新'}
                </Button>
              </div>
            </div>
          </div>
        </div>

        {/* 预留空间给未来的功能 */}
        <div className="space-y-4">
          <div className="pb-2">
            <h3 className="text-lg font-semibold text-foreground">更多功能</h3>
            <p className="text-sm text-muted-foreground mt-1">
              更多功能正在开发中...
            </p>
          </div>
          
          <div className="border-l-2 border-border pl-4 space-y-4">
            <div className="flex items-center justify-center py-8">
              <p className="text-sm text-muted-foreground">
                敬请期待更多功能的到来！
              </p>
            </div>
          </div>
        </div>
      </div>
    </div>
  )
} 