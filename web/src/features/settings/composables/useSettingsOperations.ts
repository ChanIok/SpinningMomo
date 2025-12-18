import { useToast } from '@/composables/useToast'
import { useSettingsStore } from '../store'
import type { AppSettings } from '../types'

/**
 * Settings operations with toast notifications
 * 
 * @example
 * ```vue
 * <script setup>
 * const { saveSettings, resetSettings } = useSettingsOperations()
 * 
 * const handleSave = async () => {
 *   await saveSettings()
 * }
 * </script>
 * ```
 */
export function useSettingsOperations() {
  const { toast } = useToast()
  const settingsStore = useSettingsStore()

  /**
   * Save settings with toast notification
   */
  const saveSettings = async (settings?: Partial<AppSettings>) => {
    try {
      if (settings) {
        await settingsStore.updateSettings(settings)
      }
      toast.success('设置已保存')
    } catch (error) {
      const message = error instanceof Error ? error.message : '保存失败'
      toast.error('保存设置失败', {
        description: message,
      })
      throw error
    }
  }

  /**
   * Reset settings to default
   */
  const resetSettings = async () => {
    return new Promise<void>((resolve, reject) => {
      toast('确定要重置所有设置吗？', {
        action: {
          label: '确定重置',
          onClick: async () => {
            try {
              // TODO: Implement reset logic
              toast.success('设置已重置')
              resolve()
            } catch (error) {
              const message = error instanceof Error ? error.message : '重置失败'
              toast.error('重置设置失败', {
                description: message,
              })
              reject(error)
            }
          },
        },
        cancel: {
          label: '取消',
          onClick: () => resolve(),
        },
      })
    })
  }

  /**
   * Export settings to file
   */
  const exportSettings = async () => {
    try {
      const loadingToast = toast.loading('正在导出设置...')
      
      // TODO: Implement export logic
      await new Promise((resolve) => setTimeout(resolve, 1000)) // Simulate
      
      toast.dismiss(loadingToast)
      toast.success('设置已导出')
    } catch (error) {
      const message = error instanceof Error ? error.message : '导出失败'
      toast.error('导出设置失败', {
        description: message,
      })
      throw error
    }
  }

  /**
   * Import settings from file
   */
  const importSettings = async () => {
    try {
      const loadingToast = toast.loading('正在导入设置...')
      
      // TODO: Implement import logic
      await new Promise((resolve) => setTimeout(resolve, 1000)) // Simulate
      
      toast.dismiss(loadingToast)
      toast.success('设置已导入')
    } catch (error) {
      const message = error instanceof Error ? error.message : '导入失败'
      toast.error('导入设置失败', {
        description: message,
      })
      throw error
    }
  }

  /**
   * Save settings with async promise toast
   * Automatically shows loading/success/error states
   */
  const saveSettingsWithPromise = async (settings: Partial<AppSettings>) => {
    return toast.promise(
      settingsStore.updateSettings(settings),
      {
        loading: '正在保存...',
        success: '设置已保存',
        error: (err: unknown) => {
          const message = err instanceof Error ? err.message : '保存失败'
          return `保存失败: ${message}`
        },
      }
    )
  }

  return {
    saveSettings,
    resetSettings,
    exportSettings,
    importSettings,
    saveSettingsWithPromise,
  }
}
