import type { AppSettings, CjkFontPreset, WebThemeMode } from './types'
import { resolveBackgroundImageUrl } from './backgroundPath'
import { buildOverlayGradient, getOverlayPaletteFromBackground } from './overlayPalette'

type ResolvedTheme = 'light' | 'dark'

const CJK_FONT_STACKS: Record<CjkFontPreset, string> = {
  harmony:
    "'HarmonyOS Sans SC Web', 'Microsoft YaHei UI', 'Microsoft YaHei', 'PingFang SC', 'Hiragino Sans GB', 'Noto Sans CJK SC', 'Source Han Sans SC'",
  microsoft:
    "'Microsoft YaHei UI', 'Microsoft YaHei', 'HarmonyOS Sans SC Web', 'PingFang SC', 'Hiragino Sans GB', 'Noto Sans CJK SC', 'Source Han Sans SC'",
}

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

const applyFont = (settings: AppSettings): void => {
  const root = document.documentElement
  const preset = settings.ui.webTheme.cjkFontPreset
  const stack = CJK_FONT_STACKS[preset] ?? CJK_FONT_STACKS.harmony
  root.style.setProperty('--app-font-cjk', stack)
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
    '--app-background-overlay-image',
    buildOverlayGradient(getOverlayPaletteFromBackground(background))
  )
  root.style.setProperty('--app-background-overlay-opacity', String(overlayOpacity))

  root.style.setProperty('--surface-opacity', String(clamp(background.surfaceOpacity, 0, 1)))

  if (!imageUrl) {
    root.style.setProperty('--app-background-image', 'none')
    return
  }

  root.style.setProperty('--app-background-image', `url("${imageUrl}")`)
}

export const applyAppearanceToDocument = (settings: AppSettings): void => {
  if (typeof document === 'undefined') return

  applyTheme(settings.ui.webTheme.mode)
  applyFont(settings)
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
