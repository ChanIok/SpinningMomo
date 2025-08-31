'use client'

import { useEffect } from 'react'
import { ThemeProvider as NextThemesProvider, useTheme } from 'next-themes'
import { useWebSettingsStore } from '@/lib/web-settings'

interface ThemeProviderProps {
  children: React.ReactNode
}

function ThemePersistence() {
  const { theme } = useTheme()
  const { settings, updateThemeSettings } = useWebSettingsStore()

  // 单向持久化：用户通过UI改变主题时保存到webSettings
  useEffect(() => {
    if (theme && theme !== settings.ui?.theme?.mode) {
      // 类型安全检查
      if (['light', 'dark', 'system'].includes(theme)) {
        updateThemeSettings({ mode: theme as 'light' | 'dark' | 'system' }).catch((error) => {
          console.error('Failed to persist theme settings:', error)
        })
      }
    }
  }, [theme, settings.ui?.theme?.mode, updateThemeSettings])

  return null
}

export function ThemeProvider({ children }: ThemeProviderProps) {
  const { settings } = useWebSettingsStore()

  return (
    <NextThemesProvider
      attribute='class'
      defaultTheme={settings.ui?.theme?.mode || 'system'}
      enableSystem={true}
      disableTransitionOnChange={true}
    >
      {children}
      <ThemePersistence />
    </NextThemesProvider>
  )
}
