/**
 * JSON Schema 类型定义
 * 用于解析后端返回的参数 schema
 */

// JSON Schema 基础类型
export type JSONSchemaType =
  | 'string'
  | 'number'
  | 'integer'
  | 'boolean'
  | 'object'
  | 'array'
  | 'null'

// JSON Schema 属性定义
export interface JSONSchemaProperty {
  type?: JSONSchemaType | JSONSchemaType[]
  description?: string
  default?: unknown
  enum?: unknown[]
  // 数字类型的限制
  minimum?: number
  maximum?: number
  // 字符串类型的限制
  minLength?: number
  maxLength?: number
  pattern?: string
  // 数组类型的限制
  items?: JSONSchema
  minItems?: number
  maxItems?: number
  // 对象类型的限制
  properties?: Record<string, JSONSchema>
  required?: string[]
  additionalProperties?: boolean | JSONSchema
  // 其他常用字段
  title?: string
  examples?: unknown[]
  format?: string
  // 组合 schema
  oneOf?: JSONSchema[]
  anyOf?: JSONSchema[]
  allOf?: JSONSchema[]
}

// JSON Schema 完整定义
export interface JSONSchema extends JSONSchemaProperty {
  $schema?: string
  $id?: string
  $ref?: string
  definitions?: Record<string, JSONSchema>
}

// 表单字段配置（从 schema 转换而来）
export interface FormField {
  name: string
  label: string
  type: JSONSchemaType
  required: boolean
  description?: string
  defaultValue?: unknown
  // 字符串选项
  enum?: unknown[]
  pattern?: string
  minLength?: number
  maxLength?: number
  // 数字选项
  minimum?: number
  maximum?: number
  // 数组选项
  items?: FormField
  minItems?: number
  maxItems?: number
  // 对象选项（嵌套字段）
  properties?: FormField[]
}

// 表单数据类型
export type FormData = Record<string, unknown>
