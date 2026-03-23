import type { AppSettings, WebThemeMode } from './types'
import { resolveBackgroundImageUrl } from './backgroundPath'
import { buildOverlayGradient, getOverlayPaletteFromBackground } from './overlayPalette'

const USER_CUSTOM_STYLE_ID = 'spinning-momo-user-css'
const decodedBackgroundUrls = new Set<string>()

type ResolvedTheme = 'light' | 'dark'

const HEX_COLOR_PATTERN = /^#[0-9A-Fa-f]{6}$/

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

const applyCustomUserCss = (cssText: string): void => {
  if (typeof document === 'undefined') return

  const trimmed = cssText.trim()
  let el = document.getElementById(USER_CUSTOM_STYLE_ID) as HTMLStyleElement | null
  if (!trimmed) {
    el?.remove()
    return
  }
  if (!el) {
    el = document.createElement('style')
    el.id = USER_CUSTOM_STYLE_ID
    el.setAttribute('type', 'text/css')
    document.head.appendChild(el)
  }
  el.textContent = trimmed
}

const normalizeHexColor = (value: string | undefined, fallback: string): string => {
  const normalized = value?.trim().toUpperCase() ?? ''
  return HEX_COLOR_PATTERN.test(normalized) ? normalized : fallback
}

const hexToRgb = (hexColor: string): [number, number, number] => {
  const r = Number.parseInt(hexColor.slice(1, 3), 16)
  const g = Number.parseInt(hexColor.slice(3, 5), 16)
  const b = Number.parseInt(hexColor.slice(5, 7), 16)
  return [r, g, b]
}

const toLinear = (value: number): number => {
  const normalized = value / 255
  return normalized <= 0.04045 ? normalized / 12.92 : Math.pow((normalized + 0.055) / 1.055, 2.4)
}

const resolvePrimaryForeground = (primaryColor: string): string => {
  const [r, g, b] = hexToRgb(primaryColor)
  const luminance = 0.2126 * toLinear(r) + 0.7152 * toLinear(g) + 0.0722 * toLinear(b)
  return luminance >= 0.45 ? '#18181B' : '#FFFFFF'
}

let backgroundRequestToken = 0
let appliedBackgroundUrl: string | null = null

const setBackgroundVisibility = (root: HTMLElement, visible: boolean): void => {
  root.style.setProperty('--app-background-visibility', visible ? '1' : '0')
  root.style.setProperty('--app-background-scale', visible ? '1' : '1.02')
}

const decodeImage = async (imageUrl: string): Promise<void> => {
  const image = new Image()
  image.decoding = 'async'
  image.src = imageUrl

  if (typeof image.decode === 'function') {
    await image.decode()
    return
  }

  await new Promise<void>((resolve, reject) => {
    image.onload = () => resolve()
    image.onerror = () => reject(new Error(`Failed to load background image: ${imageUrl}`))
  })
}

const applyDecodedBackgroundImage = (
  root: HTMLElement,
  imageUrl: string,
  requestToken: number
): void => {
  root.style.setProperty('--app-background-image', `url("${imageUrl}")`)

  requestAnimationFrame(() => {
    if (requestToken !== backgroundRequestToken) return
    setBackgroundVisibility(root, true)
    appliedBackgroundUrl = imageUrl
  })
}

const updateBackgroundImage = (root: HTMLElement, imageUrl: string | null): void => {
  backgroundRequestToken += 1
  const requestToken = backgroundRequestToken

  if (!imageUrl) {
    appliedBackgroundUrl = null
    root.style.setProperty('--app-background-image', 'none')
    setBackgroundVisibility(root, false)
    return
  }

  if (imageUrl === appliedBackgroundUrl) {
    setBackgroundVisibility(root, true)
    return
  }

  setBackgroundVisibility(root, false)

  if (decodedBackgroundUrls.has(imageUrl)) {
    applyDecodedBackgroundImage(root, imageUrl, requestToken)
    return
  }

  void decodeImage(imageUrl)
    .then(() => {
      decodedBackgroundUrls.add(imageUrl)
      if (requestToken !== backgroundRequestToken) return
      applyDecodedBackgroundImage(root, imageUrl, requestToken)
    })
    .catch((error: unknown) => {
      if (requestToken !== backgroundRequestToken) return
      root.style.setProperty('--app-background-image', 'none')
      setBackgroundVisibility(root, false)
      appliedBackgroundUrl = null
      console.warn('Failed to decode background image before display.', error)
    })
}

const applyBackground = (settings: AppSettings): void => {
  const root = document.documentElement
  const background = settings.ui.background
  const imageUrl = resolveBackgroundImageUrl(background)
  const resolvedTheme = resolveTheme(settings.ui.webTheme.mode)
  const primaryFallback = resolvedTheme === 'light' ? '#F59E0B' : '#FBBF24'
  const primaryColor = normalizeHexColor(background.primaryColor, primaryFallback)
  const primaryForeground = resolvePrimaryForeground(primaryColor)

  const backgroundOpacity = clamp(background.backgroundOpacity, 0, 1)
  const backgroundBlur = clamp(background.backgroundBlurAmount, 0, 100)
  const overlayOpacity = clamp(background.overlayOpacity, 0, 1)

  root.style.setProperty('--app-background-opacity', String(backgroundOpacity))
  root.style.setProperty('--app-background-blur', `${backgroundBlur}px`)
  root.style.setProperty(
    '--app-background-overlay-image',
    buildOverlayGradient(getOverlayPaletteFromBackground(background))
  )
  root.style.setProperty('--app-background-overlay-opacity', String(overlayOpacity))

  root.style.setProperty('--surface-opacity', String(clamp(background.surfaceOpacity, 0, 1)))
  root.style.setProperty('--primary', primaryColor)
  root.style.setProperty('--ring', primaryColor)
  root.style.setProperty('--sidebar-primary', primaryColor)
  root.style.setProperty('--primary-foreground', primaryForeground)
  root.style.setProperty('--sidebar-primary-foreground', primaryForeground)

  updateBackgroundImage(root, imageUrl)
}

export const applyAppearanceToDocument = (settings: AppSettings): void => {
  if (typeof document === 'undefined') return

  applyTheme(settings.ui.webTheme.mode)
  applyBackground(settings)
  applyCustomUserCss(settings.ui.webTheme.customCss ?? '')
}
