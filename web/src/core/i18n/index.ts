import { ref, shallowRef } from 'vue'
import type { Locale, LocaleDomainMessages, Messages, I18nInstance } from './types'

const DEFAULT_LOCALE: Locale = 'zh-CN'

// 当前语言
const locale = ref<Locale>(DEFAULT_LOCALE)

// 翻译字典（使用 shallowRef 避免深度响应式，提升性能）
const messages = shallowRef<Messages>({})
// 默认语言（zh-CN）降级字典
let fallbackMessages: Messages = {}

const localeLoaders = {
  'zh-CN': () => import('./locales/zh-CN'),
  'en-US': () => import('./locales/en-US'),
} satisfies Record<Locale, () => Promise<{ default: readonly LocaleDomainMessages[] }>>

/**
 * 加载并合并指定语言的所有 domain 字典
 */
async function loadLocaleMessages(targetLocale: Locale): Promise<Messages> {
  const { default: domains } = await localeLoaders[targetLocale]()

  const merged: Messages = {}
  for (const { domain, messages: domainMessages } of domains) {
    for (const [key, value] of Object.entries(domainMessages)) {
      if (key in merged) {
        console.warn(
          `[i18n] Duplicate translation key detected while loading ${targetLocale}/${domain}.json: ${key}`
        )
      }
      merged[key] = value
    }
  }
  return merged
}

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
  const text = messages.value[key] ?? fallbackMessages[key] ?? key
  return params ? interpolate(text, params) : text
}

/**
 * 切换语言
 */
async function setLocale(newLocale: Locale): Promise<void> {
  try {
    // 确保默认语言兜底字典已加载
    if (Object.keys(fallbackMessages).length === 0) {
      if (newLocale === DEFAULT_LOCALE) {
        const loaded = await loadLocaleMessages(DEFAULT_LOCALE)
        fallbackMessages = loaded
        messages.value = loaded
        locale.value = newLocale
        return
      } else {
        fallbackMessages = await loadLocaleMessages(DEFAULT_LOCALE)
      }
    }

    const loaded =
      newLocale === DEFAULT_LOCALE ? fallbackMessages : await loadLocaleMessages(newLocale)

    messages.value = loaded
    locale.value = newLocale
  } catch (error) {
    console.error(`Failed to load locale: ${newLocale}`, error)
  }
}

/**
 * 初始化 i18n
 */
export async function initI18n(initialLocale: Locale = DEFAULT_LOCALE): Promise<void> {
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
