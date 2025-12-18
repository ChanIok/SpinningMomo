import type { Ref } from 'vue'

export type Locale = 'zh-CN' | 'en-US'

export type Messages = Record<string, string>

export interface I18nInstance {
  locale: Ref<Locale>
  t: (key: string, params?: Record<string, any>) => string
  setLocale: (newLocale: Locale) => Promise<void>
}
