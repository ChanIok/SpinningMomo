// JSON-RPC 2.0 标准接口定义
export interface JsonRpcRequest {
  jsonrpc: '2.0'
  method: string
  params?: unknown
  id: string | number
}

export interface JsonRpcResponse {
  jsonrpc: '2.0'
  result?: unknown
  error?: {
    code: number
    message: string
    data?: unknown
  }
  id: string | number
}

export interface JsonRpcNotification {
  jsonrpc: '2.0'
  method: string
  params?: unknown
}

// 内部类型定义
interface PendingRequest {
  resolve: (value: unknown) => void
  reject: (error: Error) => void
  timeout: NodeJS.Timeout
}

// JSON-RPC错误码
export const JsonRpcErrorCode = {
  PARSE_ERROR: -32700,
  INVALID_REQUEST: -32600,
  METHOD_NOT_FOUND: -32601,
  INVALID_PARAMS: -32602,
  INTERNAL_ERROR: -32603,
  // 自定义错误码
  TIMEOUT: -32001,
  WEBVIEW_NOT_AVAILABLE: -32002,
} as const

export type JsonRpcErrorCode = typeof JsonRpcErrorCode[keyof typeof JsonRpcErrorCode]

export class JsonRpcError extends Error {
  code: JsonRpcErrorCode
  data?: unknown

  constructor(
    code: JsonRpcErrorCode,
    message: string,
    data?: unknown
  ) {
    super(message)
    this.name = 'JsonRpcError'
    this.code = code
    this.data = data
  }
}

// --- 模块级状态 (原单例类属性) ---
const pendingRequests = new Map<string | number, PendingRequest>()
const eventHandlers = new Map<string, Set<(params: unknown) => void>>()
let nextId = 1
const isDebugMode = import.meta.env.DEV
let isInitialized = false

// --- 内部辅助函数 (原私有方法) ---

function debug(...args: unknown[]): void {
  if (isDebugMode) {
    console.log('[WebView RPC]', ...args)
  }
}

function isWebViewAvailable(): boolean {
  return typeof window !== 'undefined' && !!window.chrome?.webview
}

function postMessage(message: JsonRpcRequest | JsonRpcNotification): void {
  if (isWebViewAvailable() && window.chrome?.webview) {
    window.chrome.webview.postMessage(message)
  } else if (isDebugMode) {
    debug('Mock message (WebView2 not available):', message)
  } else {
    throw new JsonRpcError(
      JsonRpcErrorCode.WEBVIEW_NOT_AVAILABLE,
      'WebView2 not available'
    )
  }
}

function handleResponse(response: JsonRpcResponse): void {
  const pendingRequest = pendingRequests.get(response.id)
  if (!pendingRequest) return

  const { resolve, reject, timeout } = pendingRequest
  clearTimeout(timeout)
  pendingRequests.delete(response.id)

  if (response.error) {
    const error = new JsonRpcError(
      response.error.code as JsonRpcErrorCode,
      response.error.message,
      response.error.data
    )
    reject(error)
    debug('RPC error:', response.error)
  } else {
    resolve(response.result)
    debug('RPC response:', response.id, response.result)
  }
}

function handleNotification(notification: JsonRpcNotification): void {
  const handlers = eventHandlers.get(notification.method)
  if (handlers && handlers.size > 0) {
    handlers.forEach(handler => {
      try {
        handler(notification.params)
      } catch (error) {
        console.error(`Error in event handler for ${notification.method}:`, error)
      }
    })
    debug('Event received:', notification.method, notification.params)
  } else {
    debug('No handlers for event:', notification.method)
  }
}

function isValidJsonRpcMessage(message: unknown): boolean {
  if (typeof message !== 'object' || !message) return false
  
  const msg = message as Record<string, unknown>
  
  if (msg.jsonrpc !== '2.0') return false
  
  if ('method' in msg && typeof msg.method === 'string') return true
  
  if ('id' in msg && ('result' in msg || 'error' in msg)) return true
  
  return false
}

function handleMessage(event: MessageEvent): void {
  try {
    const message = event.data

    if (!isValidJsonRpcMessage(message)) {
      debug('Invalid JSON-RPC message:', message)
      return
    }

    if ('id' in message && pendingRequests.has(message.id)) {
      handleResponse(message as JsonRpcResponse)
    }
    else if ('method' in message && !('id' in message)) {
      handleNotification(message as JsonRpcNotification)
    }
  } catch (error) {
    console.error('Failed to handle WebView message:', error)
  }
}

// --- 公开的自由函数 API ---

/**
 * 调用远程方法（请求-响应模式）
 */
export async function call<T = unknown>(method: string, params?: unknown, timeout = 10000): Promise<T> {
  return new Promise((resolve, reject) => {
    if (!isWebViewAvailable()) {
      reject(new JsonRpcError(
        JsonRpcErrorCode.WEBVIEW_NOT_AVAILABLE,
        'WebView2 not available'
      ))
      return
    }

    const id = nextId++
    const request: JsonRpcRequest = {
      jsonrpc: '2.0',
      method,
      params,
      id
    }

    const timeoutHandle = setTimeout(() => {
      pendingRequests.delete(id)
      reject(new JsonRpcError(
        JsonRpcErrorCode.TIMEOUT,
        `Request timeout: ${method}`,
        { method, timeout }
      ))
    }, timeout)

    pendingRequests.set(id, {
      resolve: (value: unknown) => resolve(value as T),
      reject,
      timeout: timeoutHandle
    })

    try {
      postMessage(request)
      debug('RPC call:', method, params)
    } catch (error) {
      clearTimeout(timeoutHandle)
      pendingRequests.delete(id)
      reject(error)
    }
  })
}

/**
 * 监听事件通知
 */
export function on(method: string, handler: (params: unknown) => void): void {
  if (!eventHandlers.has(method)) {
    eventHandlers.set(method, new Set())
  }
  eventHandlers.get(method)!.add(handler)
  debug('Event listener added:', method)
}

/**
 * 取消事件监听
 */
export function off(method: string, handler: (params: unknown) => void): void {
  const handlers = eventHandlers.get(method)
  if (handlers) {
    handlers.delete(handler)
    if (handlers.size === 0) {
      eventHandlers.delete(method)
    }
  }
  debug('Event listener removed:', method)
}

/**
 * 获取统计信息
 */
export function getStats() {
  return {
    pendingRequests: pendingRequests.size,
    eventHandlers: Array.from(eventHandlers.entries()).map(([method, handlers]) => ({
      method,
      handlerCount: handlers.size
    })),
    isWebViewAvailable: isWebViewAvailable()
  }
}

/**
 * 清理资源，在应用卸载时调用
 */
export function dispose(): void {
  for (const [, request] of pendingRequests) {
    clearTimeout(request.timeout)
    request.reject(new JsonRpcError(
      JsonRpcErrorCode.INTERNAL_ERROR,
      'WebView RPC disposed'
    ))
  }
  pendingRequests.clear()
  eventHandlers.clear()
  
  if (isWebViewAvailable() && window.chrome?.webview) {
    window.chrome.webview.removeEventListener('message', handleMessage)
  }
  
  isInitialized = false
  debug('WebView RPC disposed')
}

/**
 * 初始化 RPC 通信，应在应用启动时调用一次
 */
export function initializeRPC(): void {
  if (isInitialized) {
    debug('RPC already initialized.')
    return
  }
  
  if (isWebViewAvailable() && window.chrome?.webview) {
    window.chrome.webview.addEventListener('message', handleMessage)
    isInitialized = true
    debug('WebView RPC initialized')
  } else if (isDebugMode) {
    debug('WebView2 not available, running in mock mode')
  }

  // 确保在页面卸载时清理资源
  if (typeof window !== 'undefined') {
    window.addEventListener('beforeunload', dispose)
  }
}

// --- 类型辅助工具 ---
export type RpcMethod<TParams = unknown, TResult = unknown> = {
  params: TParams
  result: TResult
}

export type RpcEventHandler<T = unknown> = (params: T) => void