import { ref } from 'vue'
import { call } from '@/core/rpc'
import type { MethodSignatureResponse, JSONSchema, FormField } from '../types'

/**
 * 获取方法签名的 composable
 */
export function useMethodSignature() {
  const error = ref<string | null>(null)
  const signature = ref<MethodSignatureResponse | null>(null)
  const schema = ref<JSONSchema | null>(null)
  const formFields = ref<FormField[]>([])

  /**
   * 获取方法签名
   */
  const fetchSignature = async (methodName: string) => {
    error.value = null
    signature.value = null
    schema.value = null
    formFields.value = []

    try {
      const response = await call<MethodSignatureResponse>('system.methodSignature', {
        method: methodName,
      })

      signature.value = response

      // 解析 JSON Schema
      try {
        const parsedSchema = JSON.parse(response.paramsSchema) as JSONSchema
        console.log('Parsed schema:', parsedSchema)
        schema.value = parsedSchema
        const fields = parseSchemaToFormFields(parsedSchema)
        console.log('Parsed fields:', fields)
        formFields.value = fields
      } catch (parseError) {
        console.error('Failed to parse params schema:', parseError)
        error.value = '解析参数 schema 失败'
      }
    } catch (err) {
      error.value = err instanceof Error ? err.message : '获取方法签名失败'
      console.error('Failed to fetch method signature:', err)
    }
  }

  /**
   * 解析 JSON Schema 为表单字段配置
   */
  const parseSchemaToFormFields = (jsonSchema: JSONSchema): FormField[] => {
    const fields: FormField[] = []

    // 处理 $ref 引用
    let actualSchema = jsonSchema
    if (jsonSchema.$ref && jsonSchema.definitions) {
      // 解析 $ref 路径，例如 "#/definitions/SomeType"
      let refPath = jsonSchema.$ref

      // 处理不同的 $ref 格式
      if (refPath.startsWith('#/definitions/')) {
        refPath = refPath.replace('#/definitions/', '')
      } else if (refPath.startsWith('#/')) {
        refPath = refPath.replace('#/', '')
      }

      console.log('Resolving $ref:', jsonSchema.$ref, '-> refPath:', refPath)
      console.log('Available definitions:', Object.keys(jsonSchema.definitions))

      if (jsonSchema.definitions[refPath]) {
        actualSchema = jsonSchema.definitions[refPath] as JSONSchema
        console.log('Resolved schema:', actualSchema)
      } else {
        console.warn('Definition not found for:', refPath)
        console.warn('Tried to find:', refPath, 'in', jsonSchema.definitions)
      }
    }

    // 如果没有 properties，返回空数组
    if (!actualSchema.properties) {
      return fields
    }

    const requiredFields = actualSchema.required || []

    // 遍历所有属性
    for (const [propName, propSchema] of Object.entries(actualSchema.properties)) {
      const field = parsePropertyToField(propName, propSchema, requiredFields.includes(propName))
      if (field) {
        fields.push(field)
      }
    }

    return fields
  }

  /**
   * 解析单个属性为表单字段
   */
  const parsePropertyToField = (
    name: string,
    propSchema: JSONSchema,
    isRequired: boolean
  ): FormField | null => {
    // 确定字段类型
    let fieldType = propSchema.type
    if (Array.isArray(fieldType)) {
      // 如果是联合类型，取第一个非 null 的类型
      fieldType = fieldType.find((t) => t !== 'null') || fieldType[0]
    }

    if (!fieldType || typeof fieldType !== 'string') {
      console.warn(`Unknown field type for ${name}:`, fieldType)
      return null
    }

    const field: FormField = {
      name,
      label: propSchema.title || formatLabel(name),
      type: fieldType,
      required: isRequired,
      description: propSchema.description,
      defaultValue: propSchema.default,
    }

    // 根据类型添加特定配置
    switch (fieldType) {
      case 'string':
        field.enum = propSchema.enum
        field.pattern = propSchema.pattern
        field.minLength = propSchema.minLength
        field.maxLength = propSchema.maxLength
        break

      case 'number':
      case 'integer':
        field.minimum = propSchema.minimum
        field.maximum = propSchema.maximum
        break

      case 'array':
        if (propSchema.items) {
          // 简化处理：假设数组项是简单类型
          const itemType = propSchema.items.type
          if (itemType && typeof itemType === 'string') {
            field.items = {
              name: `${name}_item`,
              label: 'Item',
              type: itemType,
              required: false,
            }
          }
        }
        field.minItems = propSchema.minItems
        field.maxItems = propSchema.maxItems
        break

      case 'object':
        if (propSchema.properties) {
          // 递归解析嵌套对象
          field.properties = []
          const nestedRequired = propSchema.required || []
          for (const [nestedName, nestedSchema] of Object.entries(propSchema.properties)) {
            const nestedField = parsePropertyToField(
              nestedName,
              nestedSchema,
              nestedRequired.includes(nestedName)
            )
            if (nestedField) {
              field.properties.push(nestedField)
            }
          }
        }
        break
    }

    return field
  }

  /**
   * 格式化字段名为标签
   */
  const formatLabel = (name: string): string => {
    // 将 snake_case 或 camelCase 转换为可读的标签
    return name
      .replace(/([A-Z])/g, ' $1') // camelCase -> camel Case
      .replace(/_/g, ' ') // snake_case -> snake case
      .replace(/^./, (str) => str.toUpperCase()) // 首字母大写
      .trim()
  }

  /**
   * 生成默认参数值（基于 schema）
   */
  const generateDefaultParams = (): Record<string, unknown> => {
    const params: Record<string, unknown> = {}

    formFields.value.forEach((field) => {
      if (field.defaultValue !== undefined) {
        params[field.name] = field.defaultValue
      } else if (field.required) {
        // 为必填字段生成默认值
        params[field.name] = getDefaultValueForType(field.type)
      }
    })

    return params
  }

  /**
   * 根据类型获取默认值
   */
  const getDefaultValueForType = (type: string): unknown => {
    switch (type) {
      case 'string':
        return ''
      case 'number':
      case 'integer':
        return 0
      case 'boolean':
        return false
      case 'array':
        return []
      case 'object':
        return {}
      default:
        return null
    }
  }

  /**
   * 验证参数是否符合 schema
   */
  const validateParams = (params: unknown): { valid: boolean; errors: string[] } => {
    const errors: string[] = []

    if (!schema.value || !formFields.value.length) {
      return { valid: true, errors: [] }
    }

    // 简单验证：检查必填字段
    formFields.value.forEach((field) => {
      if (field.required) {
        const value = (params as Record<string, unknown>)[field.name]
        if (value === undefined || value === null || value === '') {
          errors.push(`字段 "${field.label}" 是必填的`)
        }
      }
    })

    return {
      valid: errors.length === 0,
      errors,
    }
  }

  return {
    error,
    signature,
    schema,
    formFields,
    fetchSignature,
    generateDefaultParams,
    validateParams,
  }
}
