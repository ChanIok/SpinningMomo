import { call } from '@/core/rpc'
import type { FeatureDescriptor } from './types'

export const featuresApi = {
  getAll: async (): Promise<FeatureDescriptor[]> => {
    const result = await call<{ features: FeatureDescriptor[] }>('features.getAll', {})
    return result.features
  },
}
