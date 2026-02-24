import type { WebBackgroundSettings } from './types'

export type OverlayColorMode = 1 | 2 | 3 | 4

export interface OverlayPalette {
  mode: OverlayColorMode
  colors: [string, string, string, string]
}

export interface OverlayPalettePreset extends OverlayPalette {
  id: string
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
  { id: 'graphite', mode: 1, colors: ['#0F1115', '#0F1115', '#0F1115', '#0F1115'] },
  { id: 'fog', mode: 2, colors: ['#6D7388', '#2B2E38', '#2B2E38', '#2B2E38'] },
  { id: 'amber', mode: 2, colors: ['#A77A2A', '#3A2A16', '#3A2A16', '#3A2A16'] },
  { id: 'teal', mode: 2, colors: ['#3A8D93', '#172D3B', '#172D3B', '#172D3B'] },
  { id: 'plum', mode: 2, colors: ['#6F5A8A', '#2B2238', '#2B2238', '#2B2238'] },
  { id: 'forest', mode: 3, colors: ['#3A6F55', '#2A3B32', '#161A20', '#161A20'] },
  { id: 'sunset', mode: 3, colors: ['#A86A3C', '#6C4D66', '#273347', '#273347'] },
  { id: 'ocean', mode: 3, colors: ['#4A76B8', '#2F5A7C', '#1E2E4A', '#1E2E4A'] },
  { id: 'dawn', mode: 4, colors: ['#A06F7E', '#7A84B0', '#2E4966', '#556C5B'] },
  { id: 'aurora', mode: 4, colors: ['#5F8A84', '#6079A8', '#393E64', '#6A4C70'] },
  { id: 'steel', mode: 4, colors: ['#76808E', '#6B7D94', '#3F4B5C', '#4E5762'] },
  { id: 'night', mode: 4, colors: ['#4E5874', '#43536B', '#242B3A', '#313544'] },
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
