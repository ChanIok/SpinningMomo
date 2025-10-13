// JSON-RPC 2.0 协议类型定义

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

// 内部类型
export interface PendingRequest {
  resolve: (value: unknown) => void
  reject: (error: Error) => void
  timeout: ReturnType<typeof setTimeout> | null
}

export interface TransportStats {
  pendingRequests: number
  eventHandlers: Array<{ method: string; handlerCount: number }>
  isConnected: boolean
  transportType: TransportType
}

export type TransportType = 'webview' | 'http'

// JSON-RPC 错误码
export const JsonRpcErrorCode = {
  PARSE_ERROR: -32700,
  INVALID_REQUEST: -32600,
  METHOD_NOT_FOUND: -32601,
  INVALID_PARAMS: -32602,
  INTERNAL_ERROR: -32603,
  TIMEOUT: -32001,
  WEBVIEW_NOT_AVAILABLE: -32002,
} as const

export type JsonRpcErrorCode = (typeof JsonRpcErrorCode)[keyof typeof JsonRpcErrorCode]

// JSON-RPC 错误类
export class JsonRpcError extends Error {
  code: JsonRpcErrorCode
  data?: unknown

  constructor(code: JsonRpcErrorCode, message: string, data?: unknown) {
    super(message)
    this.name = 'JsonRpcError'
    this.code = code
    this.data = data
  }
}
