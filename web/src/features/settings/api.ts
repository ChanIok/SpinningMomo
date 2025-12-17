
import { call } from '@/core/rpc'
import type { AppSettings } from './types'

export const settingsApi = {
  get: async (): Promise<AppSettings> => {
    return call<AppSettings>('settings.get')
  },

  update: async (settings: AppSettings): Promise<void> => {
    await call('settings.update', settings)
  }
}
