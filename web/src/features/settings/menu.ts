import type { Component } from 'vue'
import {
  Settings,
  Keyboard,
  Camera,
  Blocks,
  Monitor,
  Menu,
  Palette,
  DatabaseBackup,
  Wifi,
} from '@lucide/vue'

export type SettingsPageKey =
  | 'general'
  | 'hotkeys'
  | 'capture'
  | 'windowScene'
  | 'floatingWindow'
  | 'webAppearance'
  | 'extensions'
  | 'networkAccess'
  | 'backup'

export interface SettingsMenuItem {
  key: SettingsPageKey
  label: string
  icon: Component
}

export const SETTINGS_COMPACT_BREAKPOINT = 640

export const SETTINGS_MENUS: SettingsMenuItem[] = [
  {
    key: 'general',
    label: 'settings.layout.general.title',
    icon: Settings,
  },
  {
    key: 'hotkeys',
    label: 'settings.layout.hotkeys.title',
    icon: Keyboard,
  },
  {
    key: 'capture',
    label: 'settings.layout.capture.title',
    icon: Camera,
  },
  {
    key: 'windowScene',
    label: 'settings.layout.windowScene.title',
    icon: Monitor,
  },
  {
    key: 'floatingWindow',
    label: 'settings.layout.floatingWindow.title',
    icon: Menu,
  },
  {
    key: 'webAppearance',
    label: 'settings.layout.webAppearance.title',
    icon: Palette,
  },
  {
    key: 'extensions',
    label: 'settings.layout.extensions.title',
    icon: Blocks,
  },
  {
    // 网络共享属于低频高级能力，放在扩展与数据维护之间。
    key: 'networkAccess',
    label: 'settings.layout.networkAccess.title',
    icon: Wifi,
  },
  {
    key: 'backup',
    label: 'settings.layout.backup.title',
    icon: DatabaseBackup,
  },
]

export function isValidSettingsPageKey(key: unknown): key is SettingsPageKey {
  return typeof key === 'string' && SETTINGS_MENUS.some((item) => item.key === key)
}

export function getSettingsMenuLabelKey(key: unknown): string | undefined {
  if (typeof key !== 'string') {
    return undefined
  }
  return SETTINGS_MENUS.find((item) => item.key === key)?.label
}
