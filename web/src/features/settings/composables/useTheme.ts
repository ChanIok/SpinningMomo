import { ref, computed, watch } from 'vue'
import { useSettingsStore } from '../store'
import { storeToRefs } from 'pinia'
import type { WebThemeMode } from '../types'
import { applyAppearanceToDocument } from '../appearance'

/**
 * 主题管理 Composable
 * 负责 Web UI 的主题切换、持久化和系统主题检测
 */
export const useTheme = () => {
  const store = useSettingsStore()
  const { appSettings } = storeToRefs(store)

  // 当前实际应用的主题（解析后的 light/dark）
  const resolvedTheme = ref<'light' | 'dark'>('dark')

  // 系统主题偏好
  const systemTheme = ref<'light' | 'dark'>('dark')

  // 用户选择的主题模式
  const themeMode = computed(() => appSettings.value.ui.webTheme.mode)

  /**
   * 检测系统主题
   */
  const detectSystemTheme = (): 'light' | 'dark' => {
    if (typeof window === 'undefined') return 'dark'
    return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light'
  }

  /**
   * 解析主题模式为实际主题
   */
  const resolveTheme = (mode: WebThemeMode): 'light' | 'dark' => {
    if (mode === 'system') {
      return systemTheme.value
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
   * 监听系统主题变化
   */
  const watchSystemTheme = () => {
    if (typeof window === 'undefined') return

    const mediaQuery = window.matchMedia('(prefers-color-scheme: dark)')

    const handleChange = (e: MediaQueryListEvent) => {
      systemTheme.value = e.matches ? 'dark' : 'light'

      // 如果当前是 system 模式，重新应用主题
      if (themeMode.value === 'system') {
        syncAppearance()
      }
    }

    // 现代浏览器
    if (mediaQuery.addEventListener) {
      mediaQuery.addEventListener('change', handleChange)
    } else {
      // 旧浏览器兼容
      mediaQuery.addListener(handleChange)
    }

    return () => {
      if (mediaQuery.removeEventListener) {
        mediaQuery.removeEventListener('change', handleChange)
      } else {
        mediaQuery.removeListener(handleChange)
      }
    }
  }

  /**
   * 初始化主题
   */
  const initTheme = () => {
    // 检测系统主题
    systemTheme.value = detectSystemTheme()

    syncAppearance()

    // 监听系统主题变化
    watchSystemTheme()

    console.log('🎨 主题初始化完成:', {
      mode: themeMode.value,
      system: systemTheme.value,
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
    // 状态
    themeMode,
    resolvedTheme,
    systemTheme,

    // 方法
    setTheme,
    initTheme,
  }
}
