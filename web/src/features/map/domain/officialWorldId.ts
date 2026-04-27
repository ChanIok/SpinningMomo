export const DEFAULT_OFFICIAL_WORLD_ID = '1.1'

const OFFICIAL_WORLD_ID_PATTERN = /^\d+(?:\.\d+)?$/
const DEFAULT_OFFICIAL_WORLD_ID_BY_BASE_ID: Record<string, string> = {
  '1': '1.1',
  '10000001': '10000001.1',
  '10000002': '10000002.1',
  '10000010': '10000010.1',
  '10000027': '10000027.1',
  '4020034': '4020034.3',
}

/** localStorage 等处的 id 可能带包裹双引号，如 "1010202.1"，与规则表比较前去掉首尾 `"`。 */
export function normalizeOfficialWorldId(raw: unknown): string | undefined {
  let s = String(raw ?? '').trim()
  if (!s) {
    return undefined
  }
  if (s.length >= 2 && s[0] === '"' && s[s.length - 1] === '"') {
    s = s.slice(1, -1).trim()
  }
  if (!s || !OFFICIAL_WORLD_ID_PATTERN.test(s)) {
    return undefined
  }
  return s
}

export function normalizeOfficialWorldIdOrDefault(raw: unknown): string {
  return normalizeOfficialWorldId(raw) ?? DEFAULT_OFFICIAL_WORLD_ID
}

export function stripOfficialWorldVersion(worldId: unknown): string {
  const normalizedWorldId = normalizeOfficialWorldId(worldId)
  if (!normalizedWorldId) {
    return ''
  }
  return normalizedWorldId.split('.')[0] ?? ''
}

export function toOfficialWorldIdWithDefaultVersion(worldId: unknown): string | undefined {
  const normalizedWorldId = normalizeOfficialWorldId(worldId)
  if (!normalizedWorldId) {
    return undefined
  }
  if (normalizedWorldId.includes('.')) {
    const baseWorldId = normalizedWorldId.split('.')[0] ?? ''
    return DEFAULT_OFFICIAL_WORLD_ID_BY_BASE_ID[baseWorldId] ? normalizedWorldId : undefined
  }

  return DEFAULT_OFFICIAL_WORLD_ID_BY_BASE_ID[normalizedWorldId]
}
