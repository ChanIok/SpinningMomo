import { getStaticUrl } from '@/core/env'
import type { WebBackgroundSettings } from './types'
import { BACKGROUND_WEB_DIR } from './constants'

export const resolveBackgroundImageUrl = (background: WebBackgroundSettings): string | null => {
  if (background.type !== 'image' || !background.imageFileName) return null
  return getStaticUrl(`${BACKGROUND_WEB_DIR}/${background.imageFileName}`)
}
