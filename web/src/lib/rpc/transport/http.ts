import type { TransportMethods } from './types'
import {
  JsonRpcError,
  JsonRpcErrorCode,
  type JsonRpcRequest,
  type JsonRpcNotification,
  type TransportStats,
} from '../types'

/**
 * 创建HTTP传输方法集合
 */
export function createHttpTransport(): TransportMethods {
  const eventHandlers = new Map<string, Set<(params: unknown) => void>>()
  let nextId = 1
  const isDebugMode = import.meta.env.DEV
  let eventSource: EventSource | null = null
  let isInitialized = false

  function isHttpAvailable(): boolean {
    return typeof window !== 'undefined' && typeof fetch !== 'undefined'
  }

  function ensureEventSourceConnected(): void {
    if (!eventSource || eventSource.readyState === EventSource.CLOSED) {
      connectEventSource()
    }
  }

  function connectEventSource(): void {
    try {
      eventSource = new EventSource('/sse')

      eventSource.onmessage = (event) => {
        try {
          const message = JSON.parse(event.data)
          if (isValidJsonRpcNotification(message)) {
            handleNotification(message)
          }
        } catch (error) {
          console.error('Failed to parse SSE message:', error)
        }
      }

      eventSource.onerror = (error) => {
        console.error('SSE connection error:', error)
        // 自动重连逻辑
        setTimeout(() => {
          if (eventSource?.readyState === EventSource.CLOSED) {
            connectEventSource()
          }
        }, 5000)
      }

      eventSource.onopen = () => {
        if (isDebugMode) console.log('[HTTP RPC]', 'SSE connected')
      }
    } catch (error) {
      console.error('Failed to create EventSource:', error)
    }
  }

  function isValidJsonRpcNotification(message: unknown): boolean {
    if (typeof message !== 'object' || !message) return false

    const msg = message as Record<string, unknown>

    return (
      msg.jsonrpc === '2.0' && 'method' in msg && typeof msg.method === 'string' && !('id' in msg)
    )
  }

  function handleNotification(notification: JsonRpcNotification): void {
    const handlers = eventHandlers.get(notification.method)
    if (handlers && handlers.size > 0) {
      handlers.forEach((handler) => {
        try {
          handler(notification.params)
        } catch (error) {
          console.error(`Error in event handler for ${notification.method}:`, error)
        }
      })
      if (isDebugMode)
        console.log('[HTTP RPC]', 'Event received:', notification.method, notification.params)
    } else {
      if (isDebugMode) console.log('[HTTP RPC]', 'No handlers for event:', notification.method)
    }
  }

  // 返回TransportMethods接口实现
  return {
    call: async <T>(method: string, params?: unknown, timeout = 10000): Promise<T> => {
      return new Promise((resolve, reject) => {
        if (!isHttpAvailable()) {
          reject(new JsonRpcError(JsonRpcErrorCode.WEBVIEW_NOT_AVAILABLE, 'HTTP not available'))
          return
        }

        const id = nextId++
        const request: JsonRpcRequest = {
          jsonrpc: '2.0',
          method,
          params,
          id,
        }

        // 设置超时 (timeout=0 表示永不超时)
        const abortController = new AbortController()
        let timeoutHandle: ReturnType<typeof setTimeout> | null = null

        if (timeout > 0) {
          timeoutHandle = setTimeout(() => {
            abortController.abort()
            reject(
              new JsonRpcError(JsonRpcErrorCode.TIMEOUT, `Request timeout: ${method}`, {
                method,
                timeout,
              })
            )
          }, timeout)
        }

        // 发送HTTP请求
        fetch('/rpc', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify(request),
          signal: abortController.signal,
        })
          .then((response) => {
            if (timeoutHandle) clearTimeout(timeoutHandle)

            if (!response.ok) {
              throw new JsonRpcError(
                JsonRpcErrorCode.INTERNAL_ERROR,
                `HTTP ${response.status}: ${response.statusText}`
              )
            }

            return response.json()
          })
          .then((responseJson) => {
            if (responseJson.error) {
              reject(
                new JsonRpcError(
                  responseJson.error.code as JsonRpcErrorCode,
                  responseJson.error.message,
                  responseJson.error.data
                )
              )
            } else {
              resolve(responseJson.result as T)
            }

            if (isDebugMode) console.log('[HTTP RPC]', 'RPC response:', method, responseJson)
          })
          .catch((error) => {
            if (timeoutHandle) clearTimeout(timeoutHandle)

            if (error.name === 'AbortError') {
              // 超时已经处理过了，不需要再次reject
              return
            }

            reject(
              new JsonRpcError(
                JsonRpcErrorCode.WEBVIEW_NOT_AVAILABLE,
                `Network error: ${error.message}`,
                { method, originalError: error }
              )
            )
          })

        if (isDebugMode) console.log('[HTTP RPC]', 'RPC call:', method, params)
      })
    },

    on: (method: string, handler: (params: unknown) => void): void => {
      if (!eventHandlers.has(method)) {
        eventHandlers.set(method, new Set())
      }
      eventHandlers.get(method)!.add(handler)

      // 确保SSE连接已建立
      ensureEventSourceConnected()

      if (isDebugMode) console.log('[HTTP RPC]', 'Event listener added:', method)
    },

    off: (method: string, handler: (params: unknown) => void): void => {
      const handlers = eventHandlers.get(method)
      if (handlers) {
        handlers.delete(handler)
        if (handlers.size === 0) {
          eventHandlers.delete(method)
        }
      }
      if (isDebugMode) console.log('[HTTP RPC]', 'Event listener removed:', method)
    },

    initialize: async (): Promise<void> => {
      if (isInitialized) {
        if (isDebugMode) console.log('[HTTP RPC]', 'RPC already initialized.')
        return
      }

      if (!isHttpAvailable()) {
        throw new JsonRpcError(JsonRpcErrorCode.WEBVIEW_NOT_AVAILABLE, 'HTTP not available')
      }

      isInitialized = true
      if (isDebugMode) console.log('[HTTP RPC]', 'HTTP RPC initialized')

      // 页面卸载时清理资源
      if (typeof window !== 'undefined') {
        window.addEventListener('beforeunload', () => {
          // Clean up directly since we're in a closure
          eventHandlers.clear()

          // 关闭SSE连接
          if (eventSource) {
            eventSource.close()
            eventSource = null
          }
        })
      }
    },

    dispose: (): void => {
      // 清理事件处理器
      eventHandlers.clear()

      // 关闭SSE连接
      if (eventSource) {
        eventSource.close()
        eventSource = null
      }

      isInitialized = false
      if (isDebugMode) console.log('[HTTP RPC]', 'HTTP RPC disposed')
    },

    getStats: (): TransportStats => {
      return {
        pendingRequests: 0,
        eventHandlers: Array.from(eventHandlers.entries()).map(([method, handlers]) => ({
          method,
          handlerCount: handlers.size,
        })),
        isConnected: eventSource?.readyState === EventSource.OPEN,
        transportType: 'http',
      }
    },
  }
}
