import type { Locale } from '@/core/i18n/types'

export interface InfinityNikkiWorldOption {
  id: string
  zhCN: string
  enUS: string
}

export const INFINITY_NIKKI_WORLD_OPTIONS: InfinityNikkiWorldOption[] = [
  { id: '14000000', zhCN: '星海', enUS: 'Sea of Stars' },
  { id: '10000001', zhCN: '花焰群岛', enUS: 'Firework Isles' },
  { id: '10000002', zhCN: '无忧岛', enUS: 'Serenity Island' },
  { id: '10000010', zhCN: '丹青屿', enUS: 'Danqing Island' },
  { id: '10000027', zhCN: '丹青之境', enUS: 'Danqing Realm' },
  { id: '4020034', zhCN: '万相境', enUS: 'Wanxiang Realm' },
  { id: '8000001', zhCN: '家园', enUS: 'Home' },
  { id: '1', zhCN: '奇迹大陆', enUS: 'Miraland' },
]

export function normalizeInfinityNikkiWorldId(worldId: unknown): string {
  const value = String(worldId ?? '').trim()
  if (!value) {
    return ''
  }
  return value.split('.')[0] ?? ''
}

export function getInfinityNikkiWorldName(worldId: unknown, locale: Locale): string {
  const normalizedWorldId = normalizeInfinityNikkiWorldId(worldId)
  const option = INFINITY_NIKKI_WORLD_OPTIONS.find((item) => item.id === normalizedWorldId)
  if (!option) {
    return normalizedWorldId
  }
  return locale === 'zh-CN' ? option.zhCN : option.enUS
}
