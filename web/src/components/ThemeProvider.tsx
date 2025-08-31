'use client'

import { useEffect } from 'react'
import { ThemeProvider as NextThemesProvider, useTheme } from 'next-themes'
import { useWebSettingsStore } from '@/lib/web-settings'
import { useAppearanceActions } from '@/features/settings/hooks/useAppearanceActions'

interface ThemeProviderProps {
  children: React.ReactNode
}

function ThemePersistence() {
  const { theme } = useTheme()
  const { webSettings } = useWebSettingsStore()
  const { updateThemeSettings } = useAppearanceActions()

  // 单向持久化：用户通过UI改变主题时保存到webSettings
  useEffect(() => {
    if (theme && theme !== webSettings.ui?.theme?.mode) {
      // 类型安全检查
      if (['light', 'dark', 'system'].includes(theme)) {
        updateThemeSettings({ mode: theme as 'light' | 'dark' | 'system' }).catch((error) => {
          console.error('Failed to persist theme settings:', error)
        })
      }
    }
  }, [theme, webSettings.ui?.theme?.mode, updateThemeSettings])

  return null
}

export function ThemeProvider({ children }: ThemeProviderProps) {
  const { webSettings } = useWebSettingsStore()

  return (
    <NextThemesProvider
      attribute='class'
      defaultTheme={webSettings.ui?.theme?.mode || 'system'}
      enableSystem={true}
      disableTransitionOnChange={true}
    >
      {children}
      <ThemePersistence />
    </NextThemesProvider>
  )
}
