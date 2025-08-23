import type { TransportStats } from '../types'

export type { TransportType } from '../types'

export type TransportMethods = {
  call: <T>(method: string, params?: unknown, timeout?: number) => Promise<T>
  on: (method: string, handler: (params: unknown) => void) => void
  off: (method: string, handler: (params: unknown) => void) => void
  initialize: () => Promise<void>
  dispose: () => void
  getStats: () => TransportStats
}
