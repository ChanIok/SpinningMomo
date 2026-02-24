import { call } from '@/core/rpc'
import { isWebView } from '@/core/env'
import type { FileInfoResult, InfinityNikkiGameDirResult } from './types'

export const onboardingApi = {
  detectInfinityNikkiGameDirectory: async (): Promise<InfinityNikkiGameDirResult> => {
    return call<InfinityNikkiGameDirResult>('plugins.infinityNikki.getGameDirectory', {})
  },

  selectDirectory: async (title: string): Promise<string | null> => {
    const parentWindowMode = isWebView() ? 1 : 2
    const result = await call<{ path: string }>(
      'dialog.openDirectory',
      {
        title,
        parentWindowMode,
      },
      0
    )

    return result.path || null
  },

  getFileInfo: async (path: string): Promise<FileInfoResult> => {
    return call<FileInfoResult>('file.getInfo', { path })
  },
}
