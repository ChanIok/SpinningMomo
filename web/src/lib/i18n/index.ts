import { create } from 'zustand'
import { devtools } from 'zustand/middleware'
import { useSettingsStore } from '@/lib/settings'

// 支持的语言
export type SupportedLanguage = 'zh-CN' | 'en-US'

// 翻译资源类型
type TranslationResources = Record<string, unknown>

// i18n Store 状态
interface I18nState {
  currentLanguage: SupportedLanguage
  resources: Record<SupportedLanguage, TranslationResources>
  t: (key: string, params?: Record<string, string | number>) => string
  setLanguage: (lang: SupportedLanguage) => Promise<void>
  initialize: () => Promise<() => void>
}

// 获取翻译值
function getTranslationValue(resources: TranslationResources, key: string): string {
  const value = resources[key]

  if (value === undefined) {
    // 开发环境下警告缺失的翻译
    if (import.meta.env.DEV) {
      console.warn(`Translation missing: "${key}"`)
    }
    return key
  }

  return typeof value === 'string' ? value : key
}

// 参数插值处理
function interpolateParams(template: string, params: Record<string, string | number>): string {
  return template.replace(/\{(\w+)\}/g, (match, key) => {
    const value = params[key]
    return value !== undefined ? String(value) : match
  })
}

// 简单的复数处理（可选扩展）
function handlePlural(template: string, count: number): string {
  const parts = template.split('|')
  if (parts.length !== 2) return template

  return count === 1 ? parts[0].trim() : parts[1].trim()
}

// 加载语言的翻译资源
async function loadLanguageResources(lang: SupportedLanguage): Promise<TranslationResources> {
  try {
    // 加载所有模块
    const modules = ['common', 'about', 'settings', 'home', 'gallery']
    const resources: TranslationResources = {}

    // 并行加载所有模块
    const modulePromises = modules.map(async (module) => {
      try {
        const moduleResources = await import(`@/locales/${lang}/${module}.json`)
        return { module, resources: moduleResources.default || {} }
      } catch (error) {
        console.error(`Failed to load module ${module} for ${lang}:`, error)
        return { module, resources: {} }
      }
    })

    const moduleResults = await Promise.all(modulePromises)

    // 合并所有模块资源
    for (const { resources: moduleResources } of moduleResults) {
      Object.assign(resources, moduleResources)
    }

    return resources
  } catch (error) {
    console.error(`Failed to load language ${lang}:`, error)
    return {}
  }
}

// 创建 i18n Store
export const useI18nStore = create<I18nState>()(
  devtools(
    (set, get) => ({
      // 初始状态
      currentLanguage: 'zh-CN',
      resources: { 'zh-CN': {}, 'en-US': {} },

      // 翻译函数
      t: (key: string, params?: Record<string, string | number>) => {
        const { currentLanguage, resources } = get()
        const languageResources = resources[currentLanguage]

        if (!languageResources || Object.keys(languageResources).length === 0) {
          return key
        }

        let value = getTranslationValue(languageResources, key)

        // 处理简单复数（如果参数中有 count）
        if (params?.count !== undefined && typeof params.count === 'number') {
          value = handlePlural(value, params.count)
        }

        // 处理参数插值
        if (params && Object.keys(params).length > 0) {
          return interpolateParams(value, params)
        }

        return value
      },

      // 设置语言
      setLanguage: async (lang: SupportedLanguage) => {
        const currentState = get()

        if (currentState.currentLanguage === lang) {
          return // 语言相同，无需切换
        }

        // 立即更新当前语言
        set({ currentLanguage: lang })

        // 如果该语言资源未加载，则加载
        if (
          !currentState.resources[lang] ||
          Object.keys(currentState.resources[lang]).length === 0
        ) {
          const resources = await loadLanguageResources(lang)
          set((state) => ({
            resources: {
              ...state.resources,
              [lang]: resources,
            },
          }))
        }
      },

      // 初始化
      initialize: async () => {
        const { appSettings } = useSettingsStore.getState()
        const initialLang = appSettings.app.language.current as SupportedLanguage

        // 设置初始语言
        set({ currentLanguage: initialLang })

        // 立即加载初始语言的资源
        const resources = await loadLanguageResources(initialLang)
        set((state) => ({
          resources: {
            ...state.resources,
            [initialLang]: resources,
          },
        }))

        console.log(`✅ i18n system initialized with language: ${initialLang}`)

        // 监听设置变化
        const unsubscribe = useSettingsStore.subscribe((state) => {
          const newLang = state.appSettings.app.language.current as SupportedLanguage
          const currentStore = get()

          if (newLang && newLang !== currentStore.currentLanguage) {
            currentStore.setLanguage(newLang).catch((error) => {
              console.error('Failed to switch language:', error)
            })
          }
        })

        console.log('✅ i18n system initialized')
        return unsubscribe
      },
    }),
    {
      name: 'i18n-store',
      enabled: import.meta.env.DEV,
    }
  )
)

// 翻译 Hook
export const useTranslation = () => {
  const { t, currentLanguage } = useI18nStore()
  return { t, currentLanguage }
}
