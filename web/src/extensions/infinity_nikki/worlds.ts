import type { Locale } from '@/core/i18n/types'
import type { InfinityNikkiMapConfig, InfinityNikkiMapWorld } from './types'

export interface InfinityNikkiWorldOption {
  id: string
  officialWorldId: string
  label: string
}

export function normalizeInfinityNikkiWorldId(worldId: unknown): string {
  const value = String(worldId ?? '').trim()
  if (!value) {
    return ''
  }
  return value.split('.')[0] ?? ''
}

function pickLocalizedWorldName(world: InfinityNikkiMapWorld, locale: Locale): string {
  const primaryName = locale === 'zh-CN' ? world.name.zh : world.name.en
  const fallbackName = locale === 'zh-CN' ? world.name.en : world.name.zh
  return primaryName?.trim() || fallbackName?.trim() || world.worldId
}

export function getInfinityNikkiWorldName(
  worldId: unknown,
  locale: Locale,
  config?: InfinityNikkiMapConfig | null
): string {
  const normalizedWorldId = normalizeInfinityNikkiWorldId(worldId)
  if (!normalizedWorldId) {
    return ''
  }
  const option = config?.worlds.find((item) => item.worldId === normalizedWorldId)
  if (!option) {
    return normalizedWorldId
  }
  return pickLocalizedWorldName(option, locale)
}

export function getInfinityNikkiOfficialWorldId(
  worldId: unknown,
  config?: InfinityNikkiMapConfig | null
): string | undefined {
  const normalizedWorldId = normalizeInfinityNikkiWorldId(worldId)
  if (!normalizedWorldId) {
    return undefined
  }
  return config?.worlds.find((item) => item.worldId === normalizedWorldId)?.officialWorldId
}

export function getInfinityNikkiWorldOptions(
  config: InfinityNikkiMapConfig | null,
  locale: Locale
): InfinityNikkiWorldOption[] {
  if (!config) {
    return []
  }
  return config.worlds.map((world) => ({
    id: world.worldId,
    officialWorldId: world.officialWorldId,
    label: pickLocalizedWorldName(world, locale),
  }))
}
