import { ref } from 'vue'
import { call } from '@/core/rpc'
import type { MethodSignatureResponse, JSONSchema, FormField, JSONSchemaType } from '../types'

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
   * 统一的 $ref 解析工具
   */
  const resolveRef = (rootSchema: JSONSchema, ref: string): JSONSchema | null => {
    if (!ref.startsWith('#/')) {
      return null
    }

    // 同时支持 definitions 和 $defs (JSON Schema 2020-12)
    const defs = rootSchema.definitions ?? (rootSchema as any).$defs ?? {}

    // 处理不同的 $ref 格式
    let refPath = ref
    if (refPath.startsWith('#/definitions/')) {
      refPath = refPath.replace('#/definitions/', '')
    } else if (refPath.startsWith('#/$defs/')) {
      refPath = refPath.replace('#/$defs/', '')
    } else if (refPath.startsWith('#/')) {
      refPath = refPath.replace('#/', '')
    }

    return defs[refPath] ?? null
  }

  /**
   * 解析 JSON Schema 为表单字段配置
   */
  const parseSchemaToFormFields = (jsonSchema: JSONSchema): FormField[] => {
    const fields: FormField[] = []

    // 处理顶层 $ref 引用
    let actualSchema = jsonSchema
    if (jsonSchema.$ref) {
      const resolved = resolveRef(jsonSchema, jsonSchema.$ref)
      if (resolved) {
        actualSchema = resolved
        console.log('Resolved top-level $ref:', jsonSchema.$ref, '-> schema:', actualSchema)
      } else {
        console.warn('Definition not found for:', jsonSchema.$ref)
        console.warn('Available definitions:', Object.keys(jsonSchema.definitions || {}))
      }
    }

    // 如果没有 properties，返回空数组
    if (!actualSchema.properties) {
      return fields
    }

    const requiredFields = actualSchema.required || []

    // 遍历所有属性
    for (const [propName, propSchema] of Object.entries(actualSchema.properties)) {
      const field = parsePropertyToField(
        propName,
        propSchema,
        requiredFields.includes(propName),
        jsonSchema
      )
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
    isRequired: boolean,
    rootSchema: JSONSchema
  ): FormField | null => {
    // 1. 先处理属性级别的 $ref 引用
    let normalizedSchema = { ...propSchema }

    if (propSchema.$ref) {
      const resolved = resolveRef(rootSchema, propSchema.$ref)
      if (resolved) {
        // 合并 $ref 解析结果，保留原属性上可能声明的 title/description
        normalizedSchema = {
          ...resolved,
          title: propSchema.title ?? resolved.title,
          description: propSchema.description ?? resolved.description,
        }
        console.log(
          'Resolved property $ref:',
          name,
          propSchema.$ref,
          '-> type:',
          normalizedSchema.type
        )
      } else {
        console.warn('Definition not found for property:', name, propSchema.$ref)
      }
    }

    // 2. 处理 anyOf/oneOf，转换为标准的 type
    if (normalizedSchema.anyOf || normalizedSchema.oneOf) {
      const union = normalizedSchema.anyOf || normalizedSchema.oneOf
      // 从 anyOf/oneOf 中提取所有 type
      const types: string[] = []
      union!.forEach((subSchema) => {
        if (subSchema.type) {
          if (Array.isArray(subSchema.type)) {
            types.push(...subSchema.type)
          } else {
            types.push(subSchema.type)
          }
        }
      })

      // 去重
      const uniqueTypes = Array.from(new Set(types))

      // 如果提取到类型，归一化为 type
      if (uniqueTypes.length > 0) {
        normalizedSchema.type =
          uniqueTypes.length === 1
            ? (uniqueTypes[0] as JSONSchemaType)
            : (uniqueTypes as JSONSchemaType[])
      }
    }

    // 确定字段类型
    let fieldType = normalizedSchema.type
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
        if (normalizedSchema.properties) {
          // 递归解析嵌套对象
          field.properties = []
          const nestedRequired = normalizedSchema.required || []
          for (const [nestedName, nestedSchema] of Object.entries(normalizedSchema.properties)) {
            const nestedField = parsePropertyToField(
              nestedName,
              nestedSchema,
              nestedRequired.includes(nestedName),
              rootSchema
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
