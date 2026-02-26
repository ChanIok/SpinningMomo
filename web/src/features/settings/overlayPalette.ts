import type { WebBackgroundSettings, WebThemeMode } from './types'

export type OverlayColorMode = 1 | 2 | 3 | 4

export interface OverlayPalette {
  mode: OverlayColorMode
  colors: [string, string, string, string]
}

export interface OverlayPalettePreset extends OverlayPalette {
  id: string
  themeMode: Exclude<WebThemeMode, 'system'>
}

const DEFAULT_HEX_COLOR = '#000000'
const HEX_COLOR_PATTERN = /^#[0-9A-Fa-f]{6}$/
const MIN_OVERLAY_COLORS = 1
const MAX_OVERLAY_COLORS = 4
const DEFAULT_OVERLAY_COLORS: [string, string] = ['#000000', '#000000']

const MODE_VALUES: ReadonlyArray<OverlayColorMode> = [1, 2, 3, 4]

const normalizeMode = (mode: number | undefined): OverlayColorMode => {
  return MODE_VALUES.includes(mode as OverlayColorMode) ? (mode as OverlayColorMode) : 2
}

export const normalizeHexColor = (
  value: string | undefined,
  fallback = DEFAULT_HEX_COLOR
): string => {
  const normalized = value?.trim().toUpperCase() ?? ''
  return HEX_COLOR_PATTERN.test(normalized) ? normalized : fallback
}

const toFourColors = (colors: Array<string | undefined>): [string, string, string, string] => {
  const normalizedColors = colors.map((color) => normalizeHexColor(color, DEFAULT_HEX_COLOR))
  return [
    normalizedColors[0] ?? DEFAULT_HEX_COLOR,
    normalizedColors[1] ?? normalizedColors[0] ?? DEFAULT_HEX_COLOR,
    normalizedColors[2] ?? normalizedColors[1] ?? normalizedColors[0] ?? DEFAULT_HEX_COLOR,
    normalizedColors[3] ??
      normalizedColors[2] ??
      normalizedColors[1] ??
      normalizedColors[0] ??
      DEFAULT_HEX_COLOR,
  ]
}

const toFourColorsFromPalette = (palette: OverlayPalette): [string, string, string, string] => {
  return toFourColors(palette.colors)
}

const normalizeOverlayColors = (colors: Array<string | undefined>): string[] => {
  const normalized = colors
    .map((color) => normalizeHexColor(color, ''))
    .filter((color) => color !== '')
    .slice(0, MAX_OVERLAY_COLORS)

  if (normalized.length === 0) {
    return [...DEFAULT_OVERLAY_COLORS]
  }

  if (normalized.length < MIN_OVERLAY_COLORS) {
    return [DEFAULT_HEX_COLOR]
  }

  return normalized
}

export const OVERLAY_PALETTE_PRESETS: ReadonlyArray<OverlayPalettePreset> = [
  {
    id: 'mist',
    themeMode: 'light',
    mode: 1,
    colors: ['#F1F5FB', '#F1F5FB', '#F1F5FB', '#F1F5FB'],
  },
  {
    id: 'nikki',
    themeMode: 'light',
    mode: 2,
    colors: ['#FDF0F4', '#F9E1E6', '#F9E1E6', '#F9E1E6'],
  },
  {
    id: 'spring',
    themeMode: 'light',
    mode: 2,
    colors: ['#DAE8CA', '#F6EAD3', '#F6EAD3', '#F6EAD3'],
  },
  {
    id: 'daydream',
    themeMode: 'light',
    mode: 3,
    colors: ['#E3F0FF', '#F0E6FA', '#FBE4F0', '#FBE4F0'],
  },
  {
    id: 'graphite',
    themeMode: 'dark',
    mode: 1,
    colors: ['#171B22', '#171B22', '#171B22', '#171B22'],
  },
  {
    id: 'slate',
    themeMode: 'dark',
    mode: 1,
    colors: ['#0F172A', '#0F172A', '#0F172A', '#0F172A'],
  },
  {
    id: 'teal',
    themeMode: 'dark',
    mode: 2,
    colors: ['#0F4D4D', '#062727', '#062727', '#062727'],
  },
  {
    id: 'galaxy',
    themeMode: 'dark',
    mode: 3,
    colors: ['#0B1021', '#112240', '#1A1423', '#1A1423'],
  },
]

export const getOverlayPaletteFromBackground = (
  background: WebBackgroundSettings
): OverlayPalette => {
  const activeColors = normalizeOverlayColors(background.overlayColors)
  const mode = normalizeMode(activeColors.length)
  const colors = toFourColors(activeColors)

  return {
    mode,
    colors,
  }
}

export const getActiveOverlayColors = (palette: OverlayPalette): string[] => {
  return palette.colors.slice(0, palette.mode)
}

export const buildOverlayGradient = (palette: OverlayPalette): string => {
  const normalized = toFourColorsFromPalette(palette)
  const [c1, c2, c3, c4] = normalized

  switch (palette.mode) {
    case 1:
      return `linear-gradient(to bottom right, ${c1} 0%, ${c1} 100%)`
    case 2:
      return `linear-gradient(to bottom right, ${c1} 0%, ${c2} 100%)`
    case 3:
      return `linear-gradient(to bottom right, ${c1} 0%, ${c2} 50%, ${c3} 100%)`
    case 4:
      return [
        `radial-gradient(circle at 0% 0%, ${c1} 0%, transparent 62%)`,
        `radial-gradient(circle at 100% 0%, ${c2} 0%, transparent 62%)`,
        `radial-gradient(circle at 100% 100%, ${c3} 0%, transparent 62%)`,
        `radial-gradient(circle at 0% 100%, ${c4} 0%, transparent 62%)`,
        `linear-gradient(to bottom right, ${c1} 0%, ${c3} 100%)`,
      ].join(', ')
    default:
      return `linear-gradient(to bottom right, ${c1} 0%, ${c2} 100%)`
  }
}

export const toBackgroundOverlayPatch = (
  palette: OverlayPalette
): Pick<WebBackgroundSettings, 'overlayColors'> => {
  const mode = normalizeMode(palette.mode)
  const colors = toFourColorsFromPalette(palette)
  const activeColors = colors.slice(0, mode)

  return {
    overlayColors: activeColors,
  }
}
