import { toast } from 'sonner'
import { useSettingsStore } from '../store/settings-store'
import { ToggleSetting, SelectSetting, SettingGroup } from '../components/setting-item'
import type { PartialAppSettings } from '../types'

export function GeneralPage() {
  const {
    settings,
    isLoading,
    error,
    isInitialized,
    updateSettings
  } = useSettingsStore()

  const handleUpdateSettings = async (updates: PartialAppSettings) => {
    try {
      await updateSettings(updates)
      toast.success('设置已保存')
    } catch (error) {
      toast.error('保存设置失败')
    }
  }

  // 显示加载状态（仅在未初始化时）
  if (!isInitialized && isLoading) {
    return (
      <div className="p-6 flex items-center justify-center">
        <div className="text-center">
          <div className="h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary mx-auto"></div>
          <p className="text-sm text-muted-foreground mt-2">加载设置中...</p>
        </div>
      </div>
    )
  }

  // 显示错误状态
  if (!settings && error) {
    return (
      <div className="p-6 flex items-center justify-center">
        <div className="text-center">
          <p className="text-sm text-muted-foreground">无法加载设置</p>
          <p className="text-sm text-red-500 mt-1">{error}</p>
        </div>
      </div>
    )
  }

  // 设置尚未加载完成
  if (!settings) {
    return (
      <div className="p-6 flex items-center justify-center">
        <div className="text-center">
          <p className="text-sm text-muted-foreground">设置加载中...</p>
        </div>
      </div>
    )
  }

  const themeOptions = [
    { value: 'light', label: '浅色' },
    { value: 'dark', label: '深色' },
    { value: 'system', label: '跟随系统' }
  ]

  const languageOptions = [
    { value: 'zh', label: '中文' },
    { value: 'en', label: 'English' }
  ]

  return (
    <div className="p-6 max-w-4xl">
      {/* 页面标题 */}
      <div className="mb-6">
        <h1 className="text-2xl font-bold text-foreground">常规设置</h1>
        <p className="text-muted-foreground mt-1">
          管理应用程序的基本设置和偏好
        </p>
      </div>

      {/* 错误提示 */}
      {error && (
        <div className="mb-6 p-4 rounded-lg bg-destructive/10 border border-destructive/20">
          <p className="text-sm text-destructive">{error}</p>
        </div>
      )}

      <div className="space-y-8">
        {/* 外观设置 */}
        <SettingGroup 
          title="外观" 
          description="个性化应用程序的外观和感觉"
        >
          <SelectSetting
            title="主题"
            description="选择应用程序的颜色主题"
            value={settings.general.theme}
            onValueChange={(value) => handleUpdateSettings({ 
              general: { theme: value as 'light' | 'dark' | 'system' }
            })}
            options={themeOptions}
          />

          <SelectSetting
            title="语言"
            description="选择应用程序界面语言"
            value={settings.general.language}
            onValueChange={(value) => handleUpdateSettings({ 
              general: { language: value as 'zh' | 'en' }
            })}
            options={languageOptions}
          />
        </SettingGroup>

        {/* 启动设置 */}
        <SettingGroup 
          title="启动" 
          description="控制应用程序的启动行为"
        >
          <ToggleSetting
            title="开机自启动"
            description="系统启动时自动启动应用程序"
            checked={settings.general.autoStart}
            onCheckedChange={(checked) => handleUpdateSettings({ 
              general: { autoStart: checked }
            })}
          />

          <ToggleSetting
            title="最小化到系统托盘"
            description="关闭窗口时最小化到系统托盘而不是完全退出"
            checked={settings.general.minimizeToTray}
            onCheckedChange={(checked) => handleUpdateSettings({ 
              general: { minimizeToTray: checked }
            })}
          />
        </SettingGroup>

        {/* 通知设置 */}
        <SettingGroup 
          title="通知" 
          description="管理应用程序通知设置"
        >
          <ToggleSetting
            title="启用通知"
            description="允许应用程序显示桌面通知"
            checked={settings.general.notifications}
            onCheckedChange={(checked) => handleUpdateSettings({ 
              general: { notifications: checked }
            })}
          />
        </SettingGroup>
      </div>
    </div>
  )
} 