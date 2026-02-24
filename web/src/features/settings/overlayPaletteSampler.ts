import type { WebThemeMode } from './types'
import type { OverlayColorMode, OverlayPalette } from './overlayPalette'

type ResolvedTheme = 'light' | 'dark'

interface RgbColor {
  r: number
  g: number
  b: number
}

interface HslColor {
  h: number
  s: number
  l: number
}

export interface OverlayPaletteSampleOptions {
  imageUrl: string
  mode: OverlayColorMode
  themeMode: WebThemeMode
}

const SAMPLE_CANVAS_WIDTH = 360
const SAMPLE_CANVAS_HEIGHT = 240
const SAMPLE_BOX_SIZE = 16

const SAMPLE_POINTS: Record<OverlayColorMode, Array<[number, number]>> = {
  1: [[0.5, 0.5]],
  2: [
    [0.2, 0.2],
    [0.8, 0.8],
  ],
  3: [
    [0.18, 0.2],
    [0.5, 0.5],
    [0.82, 0.8],
  ],
  4: [
    [0.18, 0.2],
    [0.82, 0.2],
    [0.82, 0.8],
    [0.18, 0.8],
  ],
}

const clamp = (value: number, min: number, max: number): number => {
  return Math.min(max, Math.max(min, value))
}

const resolveTheme = (mode: WebThemeMode): ResolvedTheme => {
  if (mode !== 'system') {
    return mode
  }

  if (typeof window === 'undefined') {
    return 'dark'
  }

  return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light'
}

const loadImage = async (url: string): Promise<HTMLImageElement> => {
  return new Promise((resolve, reject) => {
    const image = new Image()
    image.decoding = 'async'
    image.onload = () => resolve(image)
    image.onerror = () => reject(new Error(`Failed to load wallpaper image: ${url}`))
    image.src = url
  })
}

const drawCoverImage = (
  context: CanvasRenderingContext2D,
  image: HTMLImageElement,
  canvasWidth: number,
  canvasHeight: number
) => {
  const sourceWidth = image.naturalWidth || image.width
  const sourceHeight = image.naturalHeight || image.height
  const scale = Math.max(canvasWidth / sourceWidth, canvasHeight / sourceHeight)
  const drawWidth = sourceWidth * scale
  const drawHeight = sourceHeight * scale
  const offsetX = (canvasWidth - drawWidth) / 2
  const offsetY = (canvasHeight - drawHeight) / 2

  context.clearRect(0, 0, canvasWidth, canvasHeight)
  context.drawImage(image, offsetX, offsetY, drawWidth, drawHeight)
}

const sampleAverageColor = (
  context: CanvasRenderingContext2D,
  x: number,
  y: number,
  sampleSize = SAMPLE_BOX_SIZE
): RgbColor => {
  const width = context.canvas.width
  const height = context.canvas.height
  const half = Math.floor(sampleSize / 2)
  const sampleX = clamp(Math.round(x - half), 0, width - 1)
  const sampleY = clamp(Math.round(y - half), 0, height - 1)
  const sampleWidth = Math.max(1, Math.min(sampleSize, width - sampleX))
  const sampleHeight = Math.max(1, Math.min(sampleSize, height - sampleY))
  const pixelData = context.getImageData(sampleX, sampleY, sampleWidth, sampleHeight).data

  let totalWeight = 0
  let r = 0
  let g = 0
  let b = 0

  for (let i = 0; i < pixelData.length; i += 4) {
    const alpha = pixelData[i + 3]! / 255
    if (alpha <= 0) continue

    r += pixelData[i]! * alpha
    g += pixelData[i + 1]! * alpha
    b += pixelData[i + 2]! * alpha
    totalWeight += alpha
  }

  if (totalWeight <= 0) {
    return { r: 0, g: 0, b: 0 }
  }

  return {
    r: Math.round(r / totalWeight),
    g: Math.round(g / totalWeight),
    b: Math.round(b / totalWeight),
  }
}

const rgbToHsl = ({ r, g, b }: RgbColor): HslColor => {
  const red = r / 255
  const green = g / 255
  const blue = b / 255
  const max = Math.max(red, green, blue)
  const min = Math.min(red, green, blue)
  const delta = max - min

  let h = 0
  const l = (max + min) / 2
  let s = 0

  if (delta > 0) {
    s = delta / (1 - Math.abs(2 * l - 1))

    switch (max) {
      case red:
        h = ((green - blue) / delta) % 6
        break
      case green:
        h = (blue - red) / delta + 2
        break
      default:
        h = (red - green) / delta + 4
        break
    }

    h *= 60
    if (h < 0) h += 360
  }

  return {
    h,
    s: s * 100,
    l: l * 100,
  }
}

const hslToRgb = ({ h, s, l }: HslColor): RgbColor => {
  const saturation = clamp(s, 0, 100) / 100
  const lightness = clamp(l, 0, 100) / 100
  const chroma = (1 - Math.abs(2 * lightness - 1)) * saturation
  const huePrime = (h % 360) / 60
  const x = chroma * (1 - Math.abs((huePrime % 2) - 1))

  let red = 0
  let green = 0
  let blue = 0

  if (huePrime >= 0 && huePrime < 1) {
    red = chroma
    green = x
  } else if (huePrime >= 1 && huePrime < 2) {
    red = x
    green = chroma
  } else if (huePrime >= 2 && huePrime < 3) {
    green = chroma
    blue = x
  } else if (huePrime >= 3 && huePrime < 4) {
    green = x
    blue = chroma
  } else if (huePrime >= 4 && huePrime < 5) {
    red = x
    blue = chroma
  } else {
    red = chroma
    blue = x
  }

  const m = lightness - chroma / 2

  return {
    r: Math.round((red + m) * 255),
    g: Math.round((green + m) * 255),
    b: Math.round((blue + m) * 255),
  }
}

const compensateColorForTheme = (color: RgbColor, theme: ResolvedTheme): RgbColor => {
  const hsl = rgbToHsl(color)

  if (theme === 'light') {
    return hslToRgb({
      ...hsl,
      l: clamp(hsl.l + 14, 62, 90),
    })
  }

  return hslToRgb({
    ...hsl,
    l: clamp(hsl.l - 16, 10, 34),
  })
}

const toHexPart = (value: number): string => {
  return clamp(Math.round(value), 0, 255).toString(16).toUpperCase().padStart(2, '0')
}

const rgbToHex = (color: RgbColor): string => {
  return `#${toHexPart(color.r)}${toHexPart(color.g)}${toHexPart(color.b)}`
}

const toFourColors = (colors: string[]): [string, string, string, string] => {
  const c1 = colors[0] ?? '#000000'
  const c2 = colors[1] ?? c1
  const c3 = colors[2] ?? c2
  const c4 = colors[3] ?? c3
  return [c1, c2, c3, c4]
}

export const sampleOverlayPaletteFromWallpaper = async (
  options: OverlayPaletteSampleOptions
): Promise<OverlayPalette> => {
  const image = await loadImage(options.imageUrl)
  const canvas = document.createElement('canvas')
  canvas.width = SAMPLE_CANVAS_WIDTH
  canvas.height = SAMPLE_CANVAS_HEIGHT

  const context = canvas.getContext('2d', { willReadFrequently: true })
  if (!context) {
    throw new Error('Failed to create canvas context for wallpaper sampling')
  }

  drawCoverImage(context, image, canvas.width, canvas.height)

  const theme = resolveTheme(options.themeMode)
  const points = SAMPLE_POINTS[options.mode]
  const sampledColors = points.map(([xRatio, yRatio]) => {
    const sampled = sampleAverageColor(context, canvas.width * xRatio, canvas.height * yRatio)
    const compensated = compensateColorForTheme(sampled, theme)
    return rgbToHex(compensated)
  })

  return {
    mode: options.mode,
    colors: toFourColors(sampledColors),
  }
}
