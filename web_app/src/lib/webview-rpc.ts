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

// WebView JSON-RPC 通信类，单例模式，确保全局唯一的通信实例
export class WebViewRPC {
  private static instance: WebViewRPC
  private pendingRequests = new Map<string | number, PendingRequest>()
  private eventHandlers = new Map<string, Set<(params: unknown) => void>>()
  private nextId = 1
  private isDebugMode = import.meta.env.DEV

  // 获取单例实例
  static getInstance(): WebViewRPC {
    if (!this.instance) {
      this.instance = new WebViewRPC()
    }
    return this.instance
  }

  private constructor() {
    this.initializeMessageListener()
  }

  // 初始化消息监听器
  private initializeMessageListener(): void {
    if (typeof window !== 'undefined' && window.chrome?.webview) {
      window.chrome.webview.addEventListener('message', this.handleMessage.bind(this))
      this.debug('WebView RPC initialized')
    } else if (this.isDebugMode) {
      this.debug('WebView2 not available, running in mock mode')
    }
  }

  // 调用远程方法（请求-响应模式）
  async call<T = unknown>(method: string, params?: unknown, timeout = 10000): Promise<T> {
    return new Promise((resolve, reject) => {
      // 检查WebView2可用性
      if (!this.isWebViewAvailable()) {
        reject(new JsonRpcError(
          JsonRpcErrorCode.WEBVIEW_NOT_AVAILABLE,
          'WebView2 not available'
        ))
        return
      }

      const id = this.nextId++
      const request: JsonRpcRequest = {
        jsonrpc: '2.0',
        method,
        params,
        id
      }

      // 设置超时处理
      const timeoutHandle = setTimeout(() => {
        this.pendingRequests.delete(id)
        reject(new JsonRpcError(
          JsonRpcErrorCode.TIMEOUT,
          `Request timeout: ${method}`,
          { method, timeout }
        ))
      }, timeout)

      // 存储待处理请求
      this.pendingRequests.set(id, {
        resolve: (value: unknown) => resolve(value as T),
        reject,
        timeout: timeoutHandle
      })

      // 发送请求
      try {
        this.postMessage(request)
        this.debug('RPC call:', method, params)
      } catch (error) {
        clearTimeout(timeoutHandle)
        this.pendingRequests.delete(id)
        reject(error)
      }
    })
  }

  // 监听事件通知
  on(method: string, handler: (params: unknown) => void): void {
    if (!this.eventHandlers.has(method)) {
      this.eventHandlers.set(method, new Set())
    }
    this.eventHandlers.get(method)!.add(handler)
    this.debug('Event listener added:', method)
  }

  // 取消事件监听
  off(method: string, handler: (params: unknown) => void): void {
    const handlers = this.eventHandlers.get(method)
    if (handlers) {
      handlers.delete(handler)
      if (handlers.size === 0) {
        this.eventHandlers.delete(method)
      }
    }
    this.debug('Event listener removed:', method)
  }

  // 处理收到的消息
  private handleMessage(event: MessageEvent): void {
    try {
      const message = event.data

      // 验证JSON-RPC格式
      if (!this.isValidJsonRpcMessage(message)) {
        this.debug('Invalid JSON-RPC message:', message)
        return
      }

      // 处理响应消息
      if ('id' in message && this.pendingRequests.has(message.id)) {
        this.handleResponse(message as JsonRpcResponse)
      }
      // 处理通知消息
      else if ('method' in message && !('id' in message)) {
        this.handleNotification(message as JsonRpcNotification)
      }
    } catch (error) {
      console.error('Failed to handle WebView message:', error)
    }
  }

  // 处理响应消息
  private handleResponse(response: JsonRpcResponse): void {
    const pendingRequest = this.pendingRequests.get(response.id)
    if (!pendingRequest) return

    const { resolve, reject, timeout } = pendingRequest
    clearTimeout(timeout)
    this.pendingRequests.delete(response.id)

    if (response.error) {
      const error = new JsonRpcError(
        response.error.code as JsonRpcErrorCode,
        response.error.message,
        response.error.data
      )
      reject(error)
      this.debug('RPC error:', response.error)
    } else {
      resolve(response.result)
      this.debug('RPC response:', response.id, response.result)
    }
  }

  // 处理通知消息
  private handleNotification(notification: JsonRpcNotification): void {
    const handlers = this.eventHandlers.get(notification.method)
    if (handlers && handlers.size > 0) {
      handlers.forEach(handler => {
        try {
          handler(notification.params)
        } catch (error) {
          console.error(`Error in event handler for ${notification.method}:`, error)
        }
      })
      this.debug('Event received:', notification.method, notification.params)
    } else {
      this.debug('No handlers for event:', notification.method)
    }
  }

  // 验证JSON-RPC消息格式
  private isValidJsonRpcMessage(message: unknown): boolean {
    if (typeof message !== 'object' || !message) return false
    
    const msg = message as Record<string, unknown>
    return msg.jsonrpc === '2.0' && typeof msg.method === 'string'
  }

  // 检查WebView2是否可用
  private isWebViewAvailable(): boolean {
    return typeof window !== 'undefined' && !!window.chrome?.webview
  }

  // 发送消息到WebView2
  private postMessage(message: JsonRpcRequest | JsonRpcNotification): void {
    if (this.isWebViewAvailable() && window.chrome?.webview) {
      window.chrome.webview.postMessage(message)
    } else if (this.isDebugMode) {
      // 开发模式下的模拟处理
      this.debug('Mock message (WebView2 not available):', message)
    } else {
      throw new JsonRpcError(
        JsonRpcErrorCode.WEBVIEW_NOT_AVAILABLE,
        'WebView2 not available'
      )
    }
  }

  // 调试日志
  private debug(...args: unknown[]): void {
    if (this.isDebugMode) {
      console.log('[WebView RPC]', ...args)
    }
  }

  // 获取统计信息
  getStats() {
    return {
      pendingRequests: this.pendingRequests.size,
      eventHandlers: Array.from(this.eventHandlers.entries()).map(([method, handlers]) => ({
        method,
        handlerCount: handlers.size
      })),
      isWebViewAvailable: this.isWebViewAvailable()
    }
  }

  // 清理资源
  dispose(): void {
    // 清理所有待处理请求
    for (const [, request] of this.pendingRequests) {
      clearTimeout(request.timeout)
      request.reject(new JsonRpcError(
        JsonRpcErrorCode.INTERNAL_ERROR,
        'WebView RPC disposed'
      ))
    }
    this.pendingRequests.clear()
    
    // 清理事件处理器
    this.eventHandlers.clear()
    
    this.debug('WebView RPC disposed')
  }
}

// 导出全局单例实例
export const webviewRPC = WebViewRPC.getInstance()

// 类型辅助工具
export type RpcMethod<TParams = unknown, TResult = unknown> = {
  params: TParams
  result: TResult
}

export type RpcEventHandler<T = unknown> = (params: T) => void 