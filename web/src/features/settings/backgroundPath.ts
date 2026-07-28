import { isWebView } from '@/core/env'
import type { WebBackgroundSettings } from './types'
import { BACKGROUND_WEB_DIR, BACKGROUND_WEBVIEW_ORIGIN } from './constants'

export const resolveBackgroundImageUrl = (background: WebBackgroundSettings): string | null => {
  if (background.type !== 'image' || !background.imageFileName) return null

  if (isWebView() && !import.meta.env.DEV) {
    return `${BACKGROUND_WEBVIEW_ORIGIN}/${background.imageFileName}`
  }

  return `${BACKGROUND_WEB_DIR}/${background.imageFileName}`
}
