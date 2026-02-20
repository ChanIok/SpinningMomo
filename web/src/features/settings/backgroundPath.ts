import { getStaticUrl } from '@/core/env'
import type { WebBackgroundSettings } from './types'
import { BACKGROUND_WEB_DIR, RESOURCES_WEB_ROOT } from './constants'

const HTTP_PREFIX = /^https?:\/\//i
const DRIVE_PREFIX = /^[A-Za-z]:\//

const normalizeSeparators = (value: string): string => value.replace(/\\/g, '/').trim()

export const normalizeBackgroundPath = (rawPath: string): string => {
  if (!rawPath) return ''

  const normalized = normalizeSeparators(rawPath)
  if (!normalized) return ''

  if (HTTP_PREFIX.test(normalized)) {
    return normalized
  }

  if (normalized.startsWith('/assets/')) {
    return normalized
  }

  if (normalized.startsWith('./assets/')) {
    return normalized.slice(1)
  }

  if (normalized.startsWith('assets/')) {
    return `/${normalized}`
  }

  const marker = '/resources/web/'
  const markerIndex = normalized.toLowerCase().indexOf(marker)
  if (markerIndex >= 0) {
    return `/${normalized.slice(markerIndex + marker.length)}`
  }

  if (normalized.startsWith('./resources/web/')) {
    return `/${normalized.slice('./resources/web/'.length)}`
  }

  if (normalized.startsWith('resources/web/')) {
    return `/${normalized.slice('resources/web/'.length)}`
  }

  if (normalized.startsWith('/')) {
    return normalized
  }

  return `/${normalized}`
}

export const resolveBackgroundImageUrl = (background: WebBackgroundSettings): string | null => {
  if (background.type !== 'image') return null
  if (!background.imagePath) return null

  const normalizedPath = normalizeBackgroundPath(background.imagePath)
  if (!normalizedPath) return null

  if (HTTP_PREFIX.test(normalizedPath)) {
    return normalizedPath
  }

  return getStaticUrl(normalizedPath)
}

export const toResourceFilePath = (path: string): string | null => {
  if (!path) return null

  const normalized = normalizeSeparators(path)
  if (!normalized) return null

  if (HTTP_PREFIX.test(normalized)) {
    return null
  }

  if (normalized.startsWith('./resources/web/')) {
    return normalized
  }

  if (normalized.startsWith('resources/web/')) {
    return `./${normalized}`
  }

  if (normalized.startsWith('/')) {
    return `${RESOURCES_WEB_ROOT}${normalized}`
  }

  if (DRIVE_PREFIX.test(normalized)) {
    return normalized
  }

  return `${RESOURCES_WEB_ROOT}/${normalized}`
}

export const isManagedBackgroundPath = (path: string): boolean => {
  const normalized = normalizeBackgroundPath(path)
  return normalized.startsWith(`${BACKGROUND_WEB_DIR}/`)
}
