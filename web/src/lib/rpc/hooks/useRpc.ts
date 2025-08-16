import { useState, useEffect } from 'react'
import {
  getStats,
  getTransportType,
  isConnected,
  type TransportStats,
  type TransportType,
} from '../index'

export interface RpcStatus {
  transportType: TransportType | null
  isConnected: boolean
  stats: TransportStats | null
}

/**
 * React Hook for monitoring RPC status
 */
export function useRpcStatus(refreshInterval = 1000): RpcStatus {
  const [status, setStatus] = useState<RpcStatus>({
    transportType: null,
    isConnected: false,
    stats: null,
  })

  useEffect(() => {
    const updateStatus = () => {
      try {
        setStatus({
          transportType: getTransportType(),
          isConnected: isConnected(),
          stats: getStats(),
        })
      } catch (error) {
        console.error('Failed to get RPC status:', error)
        setStatus({
          transportType: null,
          isConnected: false,
          stats: null,
        })
      }
    }

    // 立即更新一次
    updateStatus()

    // 定期更新
    const interval = setInterval(updateStatus, refreshInterval)

    return () => clearInterval(interval)
  }, [refreshInterval])

  return status
}

/**
 * React Hook for listening to RPC events
 */
export function useRpcEvent<T = unknown>(
  method: string,
  handler: (params: T) => void,
  deps: React.DependencyList = []
): void {
  useEffect(() => {
    // Import the functions directly instead of using require
    import('../index').then((module) => {
      const { on, off } = module

      const eventHandler = (params: unknown) => {
        handler(params as T)
      }

      on(method, eventHandler)

      return () => {
        off(method, eventHandler)
      }
    })
  }, [method, handler, ...deps]) // eslint-disable-line react-hooks/exhaustive-deps
}
