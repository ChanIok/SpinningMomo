import type { CSSProperties } from 'vue'

export interface LightboxFitSize {
  width: number
  height: number
}

/**
 * 计算暗房适屏模式下的缩放比例：大媒体缩小以完整落入视口，小媒体上限为 1，避免过度放大失真。
 */
export function computeLightboxFitScale(
  viewportWidth: number,
  viewportHeight: number,
  mediaWidth: number,
  mediaHeight: number
): number {
  if (viewportWidth <= 0 || viewportHeight <= 0 || mediaWidth <= 0 || mediaHeight <= 0) {
    return 1
  }

  return Math.min(viewportWidth / mediaWidth, viewportHeight / mediaHeight, 1)
}

/**
 * 按暗房适屏规则计算媒体渲染尺寸；缺少有效视口或元数据尺寸时返回 null。
 */
export function computeLightboxFitSize(
  viewportWidth: number,
  viewportHeight: number,
  mediaWidth: number,
  mediaHeight: number
): LightboxFitSize | null {
  if (viewportWidth <= 0 || viewportHeight <= 0 || mediaWidth <= 0 || mediaHeight <= 0) {
    return null
  }

  const scale = computeLightboxFitScale(viewportWidth, viewportHeight, mediaWidth, mediaHeight)
  return {
    width: Math.max(mediaWidth * scale, 1),
    height: Math.max(mediaHeight * scale, 1),
  }
}

/**
 * 生成过渡/预览图的适屏样式，使缩略图或缓存原图与正式 stage 保持像素级一致。
 * 缺少有效尺寸时退回铺满容器的 object-contain，避免按缩略图自然尺寸缩小。
 */
export function getLightboxFitImageStyle(
  viewportWidth: number,
  viewportHeight: number,
  mediaWidth: number,
  mediaHeight: number,
  objectFit: 'fill' | 'contain' = 'fill'
): CSSProperties {
  const size = computeLightboxFitSize(viewportWidth, viewportHeight, mediaWidth, mediaHeight)
  if (!size) {
    return {
      width: '100%',
      height: '100%',
      objectFit: 'contain',
    }
  }

  return {
    width: `${size.width}px`,
    height: `${size.height}px`,
    objectFit,
  }
}
