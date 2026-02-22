import type { AppSettings, WebThemeMode } from './types'
import { resolveBackgroundImageUrl } from './backgroundPath'

type ResolvedTheme = 'light' | 'dark'

const clamp = (value: number, min: number, max: number): number => {
  return Math.min(max, Math.max(min, value))
}

const detectSystemTheme = (): ResolvedTheme => {
  if (typeof window === 'undefined') return 'dark'
  return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light'
}

const resolveTheme = (mode: WebThemeMode): ResolvedTheme => {
  if (mode === 'system') {
    return detectSystemTheme()
  }
  return mode
}

const applyTheme = (mode: WebThemeMode): void => {
  const root = document.documentElement
  const resolvedTheme = resolveTheme(mode)

  if (resolvedTheme === 'dark') {
    root.classList.add('dark')
  } else {
    root.classList.remove('dark')
  }
}

const applyBackground = (settings: AppSettings): void => {
  const root = document.documentElement
  const background = settings.ui.background
  const imageUrl = resolveBackgroundImageUrl(background)

  const backgroundOpacity = clamp(background.backgroundOpacity, 0, 1)
  const backgroundBlur = clamp(background.backgroundBlurAmount, 0, 100)
  const overlayOpacity = clamp(background.overlayOpacity, 0, 1)

  root.style.setProperty('--app-background-opacity', String(backgroundOpacity))
  root.style.setProperty('--app-background-blur', `${backgroundBlur}px`)
  root.style.setProperty('--app-background-scale', '1')
  root.style.setProperty(
    '--app-background-overlay-start',
    background.overlayStartColor || '#000000'
  )
  root.style.setProperty('--app-background-overlay-end', background.overlayEndColor || '#000000')
  root.style.setProperty('--app-background-overlay-opacity', String(overlayOpacity))

  root.style.setProperty('--surface-opacity', String(clamp(background.surfaceOpacity, 0, 1)))
  root.style.setProperty('--surface-blur', `${Math.max(background.surfaceBlurAmount, 0)}px`)

  if (!imageUrl) {
    root.style.setProperty('--app-background-image', 'none')
    return
  }

  root.style.setProperty('--app-background-image', `url("${imageUrl}")`)
}

export const applyAppearanceToDocument = (settings: AppSettings): void => {
  if (typeof document === 'undefined') return

  applyTheme(settings.ui.webTheme.mode)
  applyBackground(settings)
}

export const preloadBackgroundImage = (settings: AppSettings): void => {
  if (typeof window === 'undefined') return

  const imageUrl = resolveBackgroundImageUrl(settings.ui.background)
  if (!imageUrl) return

  const image = new Image()
  image.decoding = 'async'
  image.src = imageUrl
}
