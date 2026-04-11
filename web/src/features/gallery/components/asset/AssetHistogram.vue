<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref, watch, shallowRef, nextTick } from 'vue'
import { useI18n } from '@/composables/useI18n'

interface AssetHistogramProps {
  cacheKey: string
  imageUrl: string
}

interface HistogramData {
  red: number[]
  green: number[]
  blue: number[]
  maxValue: number
}

const props = defineProps<AssetHistogramProps>()

const { t } = useI18n()

// 组件级缓存：详情面板在同一会话内反复切回同一张图时，不再重复计算。
const histogramCache = new Map<string, HistogramData>()

const histogram = shallowRef<HistogramData | null>(null)
const hasError = ref(false)
const canvasRef = ref<HTMLCanvasElement | null>(null)
const themeVersion = ref(0)

// 动画与渲染状态
let animationFrameId: number | null = null
let previousHistogram: HistogramData | null = null
let themeObserver: MutationObserver | null = null

// 通过递增任务编号丢弃过期结果，避免用户快速切图时旧结果覆盖新图。
let currentJobId = 0

function createBins(): number[] {
  return Array.from({ length: 256 }, () => 0)
}

function computeHistogram(imageData: ImageData): HistogramData {
  const red = createBins()
  const green = createBins()
  const blue = createBins()

  const pixels = imageData.data

  for (let index = 0; index < pixels.length; index += 4) {
    const alpha = pixels[index + 3] ?? 0
    if (alpha === 0) {
      continue
    }

    const r = pixels[index] ?? 0
    const g = pixels[index + 1] ?? 0
    const b = pixels[index + 2] ?? 0

    red[r] = (red[r] ?? 0) + 1
    green[g] = (green[g] ?? 0) + 1
    blue[b] = (blue[b] ?? 0) + 1
  }

  let maxValue = 1
  for (let index = 0; index < 256; index += 1) {
    maxValue = Math.max(maxValue, red[index] ?? 0, green[index] ?? 0, blue[index] ?? 0)
  }

  // 稍微放大一点 maxValue，让顶部留白，显得更优雅
  maxValue = maxValue * 1.05

  return { red, green, blue, maxValue }
}

function isDarkTheme(): boolean {
  if (typeof document === 'undefined') {
    return true
  }

  return document.documentElement.classList.contains('dark')
}

function getHistogramPalette() {
  if (isDarkTheme()) {
    return {
      red: 'rgba(248, 113, 113, 1)', // red-400 — 粉红调
      green: 'rgba(110, 231, 183, 1)', // emerald-300 — 冷青绿，避免荧光感
      blue: 'rgba(96, 165, 250, 1)', // blue-400 — 天蓝调
      compositeOperation: 'screen' as GlobalCompositeOperation,
      topAlpha: 0.9,
      bottomAlpha: 0.35,
    }
  }

  return {
    red: 'rgba(248, 113, 113, 1)', // red-400 — multiply 用亮色，重叠区呈品红而非暗泥色
    green: 'rgba(74, 222, 128, 1)', // green-400
    blue: 'rgba(96, 165, 250, 1)', // blue-400
    compositeOperation: 'multiply' as GlobalCompositeOperation,
    topAlpha: 0.9,
    bottomAlpha: 0.2,
  }
}

function waitForAnimationFrame(): Promise<void> {
  return new Promise((resolve) => window.requestAnimationFrame(() => resolve()))
}

function loadImage(url: string): Promise<HTMLImageElement> {
  return new Promise((resolve, reject) => {
    const image = new Image()
    let settled = false

    const cleanup = () => {
      image.onload = null
      image.onerror = null
    }

    image.decoding = 'async'
    image.crossOrigin = 'anonymous'
    image.onload = () => {
      if (settled) {
        return
      }
      settled = true
      cleanup()
      resolve(image)
    }
    image.onerror = () => {
      if (settled) {
        return
      }
      settled = true
      cleanup()
      reject(new Error(`Failed to load histogram image: ${url}`))
    }
    image.src = url

    if (image.complete && image.naturalWidth > 0) {
      settled = true
      cleanup()
      resolve(image)
    }
  })
}

async function refreshHistogram() {
  const cacheKey = props.cacheKey.trim()
  const imageUrl = props.imageUrl.trim()
  const jobId = ++currentJobId

  if (!cacheKey || !imageUrl) {
    histogram.value = null
    hasError.value = false
    return
  }

  const cached = histogramCache.get(cacheKey)
  if (cached) {
    histogram.value = cached
    hasError.value = false
    return
  }

  histogram.value = null
  hasError.value = false

  try {
    // 先让详情面板完成本轮渲染，再开始读图像像素，避免切图瞬间抢占主线程。
    await waitForAnimationFrame()
    const image = await loadImage(imageUrl)

    if (jobId !== currentJobId) {
      return
    }

    const canvas = document.createElement('canvas')
    canvas.width = image.naturalWidth || image.width
    canvas.height = image.naturalHeight || image.height

    const context = canvas.getContext('2d', { willReadFrequently: true })
    if (!context || canvas.width <= 0 || canvas.height <= 0) {
      throw new Error('Canvas context unavailable for histogram rendering')
    }

    // 这里直接基于已有缩略图统计，不再额外二次缩放，保持实现路径最短。
    context.drawImage(image, 0, 0, canvas.width, canvas.height)
    const imageData = context.getImageData(0, 0, canvas.width, canvas.height)

    if (jobId !== currentJobId) {
      return
    }

    const nextHistogram = computeHistogram(imageData)
    histogramCache.set(cacheKey, nextHistogram)
    histogram.value = nextHistogram
  } catch (error) {
    if (jobId !== currentJobId) {
      return
    }

    console.error('Failed to compute asset histogram:', error)
    histogram.value = null
    hasError.value = true
  }
}

function drawHistogram(canvas: HTMLCanvasElement, data: HistogramData) {
  const ctx = canvas.getContext('2d')
  if (!ctx) return

  // 高分屏适配
  const rect = canvas.getBoundingClientRect()
  const { width, height } = rect
  if (width === 0 || height === 0) return

  const dpr = window.devicePixelRatio || 1

  canvas.width = width * dpr
  canvas.height = height * dpr
  ctx.scale(dpr, dpr)
  // 不再直接设置内联 style 的宽高，这会受到 v-show (display: none) 时的影响导致宽高变为 0px 无法恢复。
  // 通过 CSS class (h-full w-full) 控制元素的渲染尺寸。

  ctx.clearRect(0, 0, width, height)

  const { maxValue } = data
  if (maxValue <= 0) return
  const normalizedMaxValue = Math.sqrt(maxValue)
  if (normalizedMaxValue <= 0) return

  const chartWidth = width
  const chartHeight = height

  const palette = getHistogramPalette()

  // 使用原始 bins 直接绘制，并对纵轴做平方根压缩，避免尖峰把整张图压扁。
  const drawArea = (bins: number[], color: string) => {
    const getY = (value: number) => {
      return chartHeight - (Math.sqrt(Math.max(0, value)) / normalizedMaxValue) * chartHeight
    }

    ctx.beginPath()
    ctx.moveTo(0, chartHeight)

    const step = chartWidth / (bins.length - 1)

    // 起点
    ctx.lineTo(0, getY(bins[0] ?? 0))

    for (let i = 1; i < bins.length; i += 1) {
      ctx.lineTo(i * step, getY(bins[i] ?? 0))
    }

    ctx.lineTo(chartWidth, chartHeight)
    ctx.closePath()

    const gradient = ctx.createLinearGradient(0, 0, 0, chartHeight)
    const topColor = color.replace(/[\d.]+\)$/, `${palette.topAlpha})`)
    const bottomColor = color.replace(/[\d.]+\)$/, `${palette.bottomAlpha})`)
    gradient.addColorStop(0, topColor)
    gradient.addColorStop(1, bottomColor)

    ctx.fillStyle = gradient
    ctx.fill()
  }

  ctx.globalCompositeOperation = palette.compositeOperation
  drawArea(data.red, palette.red)
  drawArea(data.green, palette.green)
  drawArea(data.blue, palette.blue)
  ctx.globalCompositeOperation = 'source-over'
}

watch(histogram, async (newData) => {
  if (!newData || !canvasRef.value) return

  await nextTick() // 等待 DOM 渲染完毕，获取到带有具体宽高（而非由于 v-show 或隐式隐藏导致 0px）的元素

  const canvas = canvasRef.value
  if (!canvas) return

  if (animationFrameId) {
    cancelAnimationFrame(animationFrameId)
    animationFrameId = null
  }

  if (!previousHistogram) {
    drawHistogram(canvas, newData)
    previousHistogram = newData
    return
  }

  const startAt = performance.now()
  const prev = previousHistogram

  // Spring animation parameters
  const frequency = 8 // rad/s
  const damping = 7
  const maxMs = 1200
  const restDelta = 0.001

  const springProgress = (tSec: number) => {
    const w = frequency
    const d = damping
    const exp = Math.exp(-d * tSec)
    const value = 1 - exp * (Math.cos(w * tSec) + (d / w) * Math.sin(w * tSec))
    return Math.max(0, Math.min(1, value))
  }

  const lerpArray = (from: number[], to: number[], p: number) =>
    from.map((v, i) => v + ((to[i] ?? 0) - v) * p)

  const frame = (now: number) => {
    const elapsedMs = now - startAt
    const tSec = elapsedMs / 1000
    const eased = springProgress(tSec)

    const interpolated: HistogramData = {
      red: lerpArray(prev.red, newData.red, eased),
      green: lerpArray(prev.green, newData.green, eased),
      blue: lerpArray(prev.blue, newData.blue, eased),
      maxValue: prev.maxValue + (newData.maxValue - prev.maxValue) * eased,
    }

    drawHistogram(canvas, interpolated)

    const done = Math.abs(1 - eased) < restDelta || elapsedMs >= maxMs
    if (!done) {
      animationFrameId = requestAnimationFrame(frame)
    } else {
      previousHistogram = newData
      animationFrameId = null
      // 最后一帧确保准确
      drawHistogram(canvas, newData)
    }
  }

  animationFrameId = requestAnimationFrame(frame)
})

watch(themeVersion, async () => {
  if (!histogram.value || !canvasRef.value) return

  if (animationFrameId) {
    cancelAnimationFrame(animationFrameId)
    animationFrameId = null
  }

  await nextTick()
  drawHistogram(canvasRef.value, histogram.value)
})

watch(
  () => [props.cacheKey, props.imageUrl] as const,
  () => void refreshHistogram(),
  {
    immediate: true,
  }
)

onMounted(() => {
  if (typeof document === 'undefined') {
    return
  }

  themeObserver = new MutationObserver(() => {
    themeVersion.value += 1
  })
  themeObserver.observe(document.documentElement, {
    attributes: true,
    attributeFilter: ['class', 'style'],
  })
})

onBeforeUnmount(() => {
  currentJobId += 1
  if (animationFrameId) {
    cancelAnimationFrame(animationFrameId)
    animationFrameId = null
  }
  themeObserver?.disconnect()
  themeObserver = null
})
</script>

<template>
  <div class="space-y-3">
    <div class="flex items-center justify-between gap-2">
      <h4 class="text-sm font-medium">{{ t('gallery.details.histogram.title') }}</h4>
    </div>

    <div
      class="group relative h-28 w-full overflow-hidden rounded-md border [border-color:inherit] bg-transparent shadow-none transition-all duration-300"
    >
      <!-- Canvas 渲染层 -->
      <canvas
        ref="canvasRef"
        class="absolute inset-0 h-full w-full transition-opacity duration-500"
        :class="histogram && !hasError ? 'opacity-100' : 'opacity-0'"
      />

      <div
        v-if="hasError || !histogram"
        class="absolute inset-0 flex items-center justify-center px-4 text-center text-xs text-muted-foreground"
      >
        {{
          hasError
            ? t('gallery.details.histogram.unavailable')
            : t('gallery.details.histogram.empty')
        }}
      </div>
    </div>
  </div>
</template>
