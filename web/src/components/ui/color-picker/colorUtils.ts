/**
 * Color utility functions for custom color picker.
 * Focuses on Hex <-> HSV conversions.
 */

export interface HSV {
  h: number // 0-360
  s: number // 0-100
  v: number // 0-100
}

/**
 * Converts a HEX color string to HSV object.
 * @param hex A HEX color string (e.g., '#FF0000' or 'FF0000')
 * @returns HSV object
 */
export const hexToHsv = (hex: string): HSV => {
  let r = 0,
    g = 0,
    b = 0

  if (hex.startsWith('#')) {
    hex = hex.slice(1)
  }

  if (hex.length === 3) {
    r = parseInt(hex.charAt(0) + hex.charAt(0), 16)
    g = parseInt(hex.charAt(1) + hex.charAt(1), 16)
    b = parseInt(hex.charAt(2) + hex.charAt(2), 16)
  } else if (hex.length === 6) {
    r = parseInt(hex.slice(0, 2), 16)
    g = parseInt(hex.slice(2, 4), 16)
    b = parseInt(hex.slice(4, 6), 16)
  }

  r /= 255
  g /= 255
  b /= 255

  const max = Math.max(r, g, b)
  const min = Math.min(r, g, b)
  const d = max - min

  let h = 0
  const s = max === 0 ? 0 : d / max
  const v = max

  if (max !== min) {
    switch (max) {
      case r:
        h = (g - b) / d + (g < b ? 6 : 0)
        break
      case g:
        h = (b - r) / d + 2
        break
      case b:
        h = (r - g) / d + 4
        break
    }
    h /= 6
  }

  return {
    h: Math.round(h * 360),
    s: Math.round(s * 100),
    v: Math.round(v * 100),
  }
}

/**
 * Converts an HSV object to a HEX color string.
 * @param hsv HSV object
 * @returns A HEX color string (e.g., '#FF0000')
 */
export const hsvToHex = ({ h, s, v }: HSV): string => {
  h = h / 360
  s = s / 100
  v = v / 100

  let r = 0,
    g = 0,
    b = 0

  const i = Math.floor(h * 6)
  const f = h * 6 - i
  const p = v * (1 - s)
  const q = v * (1 - f * s)
  const t = v * (1 - (1 - f) * s)

  switch (i % 6) {
    case 0:
      r = v
      g = t
      b = p
      break
    case 1:
      r = q
      g = v
      b = p
      break
    case 2:
      r = p
      g = v
      b = t
      break
    case 3:
      r = p
      g = q
      b = v
      break
    case 4:
      r = t
      g = p
      b = v
      break
    case 5:
      r = v
      g = p
      b = q
      break
  }

  const toHex = (c: number) => {
    const hexVal = Math.round(c * 255).toString(16)
    return hexVal.length === 1 ? '0' + hexVal : hexVal
  }

  return `#${toHex(r)}${toHex(g)}${toHex(b)}`.toUpperCase()
}

/**
 * Normalizes an arbitrary string into a valid HEX color.
 * If invalid, returns the fallback color.
 */
export const normalizeToHex = (value: string, fallback: string = '#000000'): string => {
  const normalized = value.trim().toUpperCase()
  const hexPattern = /^#[0-9A-F]{6}$/
  return hexPattern.test(normalized) ? normalized : fallback
}
