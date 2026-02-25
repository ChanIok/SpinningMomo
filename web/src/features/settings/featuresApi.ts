import { call } from '@/core/rpc'
import type { FeatureDescriptor } from './types'

export const featuresApi = {
  getAll: async (): Promise<FeatureDescriptor[]> => {
    const result = await call<{ commands: FeatureDescriptor[] }>('commands.getAll', {})
    return result.commands
  },

  invoke: async (id: string): Promise<void> => {
    const result = await call<{ success: boolean; message: string }>('commands.invoke', { id })
    if (!result.success) {
      throw new Error(result.message || `调用命令失败: ${id}`)
    }
  },
}
