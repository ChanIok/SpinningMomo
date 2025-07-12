import { toast } from 'sonner'
import { useSettingsStore } from '../store/settings-store'
import { ToggleSetting, SelectSetting, NumberSetting, SettingGroup } from '../components/setting-item'
import type { PartialAppSettings } from '../types'

export function AdvancedPage() {
  const {
    settings,
    isLoading,
    error,
    isInitialized,
    updateSettings,
    resetSettings
  } = useSettingsStore()

  const handleUpdateSettings = async (updates: PartialAppSettings) => {
    try {
      await updateSettings(updates)
      toast.success('设置已保存')
    } catch (error) {
      toast.error('保存设置失败')
    }
  }

  const handleResetSettings = async () => {
    try {
      await resetSettings()
      toast.success('设置已重置为默认值')
    } catch (error) {
      toast.error('重置设置失败')
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

  const logLevelOptions = [
    { value: 'error', label: '错误' },
    { value: 'warn', label: '警告' },
    { value: 'info', label: '信息' },
    { value: 'debug', label: '调试' }
  ]

  return (
    <div className="p-6 max-w-4xl">
      {/* 页面标题 */}
      <div className="mb-6">
        <h1 className="text-2xl font-bold text-foreground">高级设置</h1>
        <p className="text-muted-foreground mt-1">
          配置应用程序的高级功能和调试选项
        </p>
      </div>

      {/* 错误提示 */}
      {error && (
        <div className="mb-6 p-4 rounded-lg bg-destructive/10 border border-destructive/20">
          <p className="text-sm text-destructive">{error}</p>
        </div>
      )}

      <div className="space-y-8">
        {/* 调试设置 */}
        <SettingGroup 
          title="调试" 
          description="开发和调试相关的设置"
        >
          <ToggleSetting
            title="调试模式"
            description="启用调试模式以获取更详细的日志信息"
            checked={settings.advanced.debugMode}
            onCheckedChange={(checked) => handleUpdateSettings({ 
              advanced: { debugMode: checked }
            })}
          />

          <SelectSetting
            title="日志级别"
            description="设置应用程序的日志详细程度"
            value={settings.advanced.logLevel}
            onValueChange={(value) => handleUpdateSettings({ 
              advanced: { logLevel: value as 'error' | 'warn' | 'info' | 'debug' }
            })}
            options={logLevelOptions}
          />

          <NumberSetting
            title="最大日志文件数"
            description="保留的日志文件数量上限"
            value={settings.advanced.maxLogFiles}
            onValueChange={(value: number) => handleUpdateSettings({ 
              advanced: { maxLogFiles: value }
            })}
            min={1}
            max={100}
          />
        </SettingGroup>

        {/* 性能设置 */}
        <SettingGroup 
          title="性能" 
          description="调整应用程序的性能设置"
        >
          <ToggleSetting
            title="性能模式"
            description="启用性能优化，可能会增加资源消耗"
            checked={settings.advanced.performanceMode}
            onCheckedChange={(checked) => handleUpdateSettings({ 
              advanced: { performanceMode: checked }
            })}
          />
        </SettingGroup>

        {/* 实验性功能 */}
        <SettingGroup 
          title="实验性功能" 
          description="尝试新的实验性功能（可能不稳定）"
        >
          <ToggleSetting
            title="启用实验性功能"
            description="允许使用实验性功能，可能会影响稳定性"
            checked={settings.advanced.experimentalFeatures}
            onCheckedChange={(checked) => handleUpdateSettings({ 
              advanced: { experimentalFeatures: checked }
            })}
          />
        </SettingGroup>

        {/* 重置设置 */}
        <SettingGroup 
          title="重置" 
          description="将所有设置恢复到默认值"
        >
          <div className="flex items-center justify-between p-4 rounded-lg border border-border">
            <div>
              <h3 className="text-sm font-medium text-foreground">重置所有设置</h3>
              <p className="text-sm text-muted-foreground">
                将应用程序的所有设置恢复到初始状态
              </p>
            </div>
            <button
              onClick={handleResetSettings}
              disabled={isLoading}
              className="px-4 py-2 text-sm font-medium text-destructive bg-destructive/10 border border-destructive/20 rounded-md hover:bg-destructive/20 disabled:opacity-50 disabled:cursor-not-allowed"
            >
              {isLoading ? '重置中...' : '重置设置'}
            </button>
          </div>
        </SettingGroup>
      </div>
    </div>
  )
} 