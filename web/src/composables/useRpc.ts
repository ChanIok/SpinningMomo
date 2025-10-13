import { ref, onMounted, onUnmounted } from 'vue'
import {
  getStats,
  getTransportType,
  isConnected,
  on,
  off,
  type TransportStats,
  type TransportType,
} from '@/core/rpc'

export interface RpcStatus {
  transportType: TransportType | null
  isConnected: boolean
  stats: TransportStats | null
}

/**
 * Vue Composable for monitoring RPC status
 */
export function useRpcStatus(refreshInterval = 1000) {
  const transportType = ref<TransportType | null>(null)
  const isConnectedRef = ref(false)
  const stats = ref<TransportStats | null>(null)

  let intervalId: ReturnType<typeof setInterval> | null = null

  const updateStatus = () => {
    try {
      transportType.value = getTransportType()
      isConnectedRef.value = isConnected()
      stats.value = getStats()
    } catch (error) {
      console.error('Failed to get RPC status:', error)
      transportType.value = null
      isConnectedRef.value = false
      stats.value = null
    }
  }

  onMounted(() => {
    // 立即更新一次
    updateStatus()

    // 定期更新
    intervalId = setInterval(updateStatus, refreshInterval)
  })

  onUnmounted(() => {
    if (intervalId) {
      clearInterval(intervalId)
    }
  })

  return {
    transportType,
    isConnected: isConnectedRef,
    stats,
  }
}

/**
 * Vue Composable for listening to RPC events
 */
export function useRpcEvent<T = unknown>(method: string, handler: (params: T) => void): void {
  const eventHandler = (params: unknown) => {
    handler(params as T)
  }

  onMounted(() => {
    on(method, eventHandler)
  })

  onUnmounted(() => {
    off(method, eventHandler)
  })
}
