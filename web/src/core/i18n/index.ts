import { ref, shallowRef } from 'vue'
import type { Locale, Messages, I18nInstance } from './types'

// 当前语言
const locale = ref<Locale>('zh-CN')

// 翻译字典（使用 shallowRef 避免深度响应式，提升性能）
const messages = shallowRef<Messages>({})

/**
 * 参数插值：替换文本中的 {key} 占位符
 */
function interpolate(text: string, params: Record<string, any>): string {
  return text.replace(/\{(\w+)\}/g, (_, key) => {
    return params[key] !== undefined ? String(params[key]) : `{${key}}`
  })
}

/**
 * 翻译函数
 */
function t(key: string, params?: Record<string, any>): string {
  const text = messages.value[key] || key
  return params ? interpolate(text, params) : text
}

/**
 * 切换语言
 */
async function setLocale(newLocale: Locale): Promise<void> {
  try {
    // 动态导入对应的语言文件
    const module = await import(`./locales/${newLocale}.json`)
    messages.value = module.default || module
    locale.value = newLocale
  } catch (error) {
    console.error(`Failed to load locale: ${newLocale}`, error)
  }
}

/**
 * 初始化 i18n
 */
export async function initI18n(initialLocale: Locale = 'zh-CN'): Promise<void> {
  await setLocale(initialLocale)
}

/**
 * useI18n composable
 */
export function useI18n(): I18nInstance {
  return {
    locale,
    t,
    setLocale,
  }
}
