// API 方法信息
export interface ApiMethod {
  name: string
  description?: string
  params?: ApiParam[]
  category?: string
}

// API 参数信息
export interface ApiParam {
  name: string
  type: string
  required: boolean
  description?: string
  defaultValue?: unknown
}

// API 测试请求
export interface ApiTestRequest {
  method: string
  params?: unknown
}

// API 测试响应
export interface ApiTestResponse {
  success: boolean
  data?: unknown
  error?: {
    code: number
    message: string
    data?: unknown
  }
  timestamp: number
  duration: number
}

// API 测试历史记录
export interface ApiTestHistory {
  id: string
  request: ApiTestRequest
  response: ApiTestResponse
  timestamp: number
}

// 方法签名响应
export interface MethodSignatureResponse {
  method: string
  description: string
  paramsSchema: string  // JSON Schema 字符串
}

// 导出 schema 相关类型
export type { JSONSchema, FormField, FormData, JSONSchemaType } from './schema'
