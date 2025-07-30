import { toast } from 'sonner'
import { useSettingsStore } from '@/lib/settings'
import { Label } from '@/components/ui/label'
import { 
  Select, 
  SelectContent, 
  SelectItem, 
  SelectTrigger, 
  SelectValue 
} from '@/components/ui/select'
import { HotkeyRecorder } from './hotkey-recorder'

export function SettingsContent() {
  const { appSettings, updateSettings, isLoading } = useSettingsStore()
  
  // 重置为默认值
  const handleReset = async () => {
    try {
      await updateSettings({
        app: {
          ...appSettings.app,
          language: {
            current: 'zh-CN'
          },
          logger: {
            level: 'INFO'
          },
          hotkey: {
            toggleVisibility: {
              modifiers: 3, // Ctrl + Alt
              key: 82 // R键
            },
            screenshot: {
              modifiers: 0, // 无修饰键
              key: 44 // 印屏键
            }
          }
        }
      });
      toast.success('设置已重置为默认值');
    } catch (error) {
      console.error('Failed to reset settings:', error);
      toast.error('重置设置失败');
    }
  };

  return (
    <div className="p-6 w-full">
      <div className="mb-6">
        <h1 className="text-2xl font-bold text-foreground">通用设置</h1>
        <p className="text-muted-foreground mt-1">
          管理应用程序的核心设置
        </p>
      </div>
      
      <div className="space-y-8">
        {/* 语言设置 */}
        <div className="space-y-4">
          <div className="pb-2">
            <h3 className="text-lg font-semibold text-foreground">语言设置</h3>
            <p className="text-sm text-muted-foreground mt-1">
              选择应用程序的显示语言
            </p>
          </div>
          
          <div className="border-l-2 border-border pl-4 space-y-4">
            <div className="flex items-center justify-between py-4">
              <div className="flex-1 pr-4">
                <Label className="text-sm font-medium text-foreground">
                  显示语言
                </Label>
                <p className="text-sm text-muted-foreground mt-1">
                  更改应用程序界面的语言
                </p>
              </div>
              <div className="flex-shrink-0 w-48">
                <Select 
                  value={appSettings.app.language.current} 
                  onValueChange={(value) => updateSettings({ 
                    app: { 
                      ...appSettings.app, 
                      language: { current: value } 
                    } 
                  })}
                >
                  <SelectTrigger>
                    <SelectValue placeholder="选择语言" />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value="zh-CN">简体中文</SelectItem>
                    <SelectItem value="en-US">English</SelectItem>
                  </SelectContent>
                </Select>
              </div>
            </div>
          </div>
        </div>
        
        {/* 日志设置 */}
        <div className="space-y-4">
          <div className="pb-2">
            <h3 className="text-lg font-semibold text-foreground">日志设置</h3>
            <p className="text-sm text-muted-foreground mt-1">
              配置应用程序的日志记录级别
            </p>
          </div>
          
          <div className="border-l-2 border-border pl-4 space-y-4">
            <div className="flex items-center justify-between py-4">
              <div className="flex-1 pr-4">
                <Label className="text-sm font-medium text-foreground">
                  日志级别
                </Label>
                <p className="text-sm text-muted-foreground mt-1">
                  控制记录的日志详细程度
                </p>
              </div>
              <div className="flex-shrink-0 w-48">
                <Select 
                  value={appSettings.app.logger.level} 
                  onValueChange={(value) => updateSettings({ 
                    app: { 
                      ...appSettings.app, 
                      logger: { level: value } 
                    } 
                  })}
                >
                  <SelectTrigger>
                    <SelectValue placeholder="选择日志级别" />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value="DEBUG">调试 (DEBUG)</SelectItem>
                    <SelectItem value="INFO">信息 (INFO)</SelectItem>
                    <SelectItem value="ERROR">错误 (ERROR)</SelectItem>
                  </SelectContent>
                </Select>
              </div>
            </div>
          </div>
        </div>
        
        {/* 快捷键设置 */}
        <div className="space-y-4">
          <div className="pb-2">
            <h3 className="text-lg font-semibold text-foreground">快捷键设置</h3>
            <p className="text-sm text-muted-foreground mt-1">
              自定义应用程序的快捷键
            </p>
          </div>
          
          <div className="border-l-2 border-border pl-4 space-y-4">
            <div className="flex items-center justify-between py-4">
              <div className="flex-1 pr-4">
                <Label className="text-sm font-medium text-foreground">
                  浮窗显示/隐藏快捷键
                </Label>
                <p className="text-sm text-muted-foreground mt-1">
                  设置显示或隐藏浮窗的快捷键
                </p>
              </div>
              <div className="flex-shrink-0 w-48">
                <HotkeyRecorder 
                  value={{
                    modifiers: appSettings.app.hotkey.toggleVisibility.modifiers,
                    key: appSettings.app.hotkey.toggleVisibility.key
                  }} 
                  onChange={(newHotkey) => updateSettings({ 
                    app: { 
                      ...appSettings.app, 
                      hotkey: {
                        ...appSettings.app.hotkey,
                        toggleVisibility: newHotkey
                      }
                    } 
                  })}
                />
              </div>
            </div>
            
            <div className="flex items-center justify-between py-4">
              <div className="flex-1 pr-4">
                <Label className="text-sm font-medium text-foreground">
                  截图快捷键
                </Label>
                <p className="text-sm text-muted-foreground mt-1">
                  设置触发截图功能的快捷键
                </p>
              </div>
              <div className="flex-shrink-0 w-48">
                <HotkeyRecorder 
                  value={{
                    modifiers: appSettings.app.hotkey.screenshot.modifiers,
                    key: appSettings.app.hotkey.screenshot.key
                  }} 
                  onChange={(newHotkey) => updateSettings({ 
                    app: { 
                      ...appSettings.app, 
                      hotkey: {
                        ...appSettings.app.hotkey,
                        screenshot: newHotkey
                      }
                    } 
                  })}
                />
              </div>
            </div>
            
            <div className="py-2 text-sm text-muted-foreground">
              <p>说明:</p>
              <ul className="list-disc pl-5 mt-1 space-y-1">
                <li>点击输入框后按下要设置的快捷键组合</li>
                <li>按 Backspace 键清除快捷键</li>
                <li>按 Esc 键取消设置</li>
              </ul>
            </div>
          </div>
        </div>
        
        {/* 重置按钮 */}
        <div className="flex justify-end gap-3 py-4">
          <button
            onClick={handleReset}
            disabled={isLoading}
            className="px-4 py-2 bg-secondary text-secondary-foreground rounded-md text-sm font-medium hover:bg-secondary/80 transition-colors disabled:opacity-50"
          >
            重置默认值
          </button>
        </div>
      </div>
    </div>
  )
}