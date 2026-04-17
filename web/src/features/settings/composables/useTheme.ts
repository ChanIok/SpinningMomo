import { ref, computed, watch } from 'vue'
import { useSettingsStore } from '../store'
import { storeToRefs } from 'pinia'
import type { WebThemeMode } from '../types'
import { applyAppearanceToDocument } from '../appearance'

/**
 * 主题管理 Composable
 * 负责 Web UI 的主题切换与持久化（仅 light / dark；历史 system 按亮色解析）
 */
export const useTheme = () => {
  const store = useSettingsStore()
  const { appSettings } = storeToRefs(store)

  // 当前实际应用的主题（解析后的 light/dark）
  const resolvedTheme = ref<'light' | 'dark'>('dark')

  // 用户选择的主题模式
  const themeMode = computed(() => appSettings.value.ui.webTheme.mode)

  const resolveTheme = (mode: WebThemeMode): 'light' | 'dark' => {
    if (mode === 'system') {
      return 'light'
    }
    return mode
  }

  const syncAppearance = () => {
    applyAppearanceToDocument(appSettings.value)
    resolvedTheme.value = resolveTheme(themeMode.value)
  }

  /**
   * 更新主题模式
   */
  const setTheme = async (mode: WebThemeMode) => {
    try {
      await store.updateSettings({
        ...appSettings.value,
        ui: {
          ...appSettings.value.ui,
          webTheme: {
            ...appSettings.value.ui.webTheme,
            mode,
          },
        },
      })

      syncAppearance()

      console.log('✅ 主题已更新:', mode, '→', resolvedTheme.value)
    } catch (error) {
      console.error('❌ 更新主题失败:', error)
      throw error
    }
  }

  /**
   * 初始化主题
   */
  const initTheme = () => {
    syncAppearance()

    console.log('🎨 主题初始化完成:', {
      mode: themeMode.value,
      resolved: resolvedTheme.value,
    })
  }

  // 监听主题模式变化
  watch(
    () => themeMode.value,
    () => {
      syncAppearance()
    }
  )

  return {
    themeMode,
    resolvedTheme,

    setTheme,
    initTheme,
  }
}
