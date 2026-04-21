/** localStorage 等处的 id 可能带包裹双引号，如 "1010202.1"，与规则表比较前去掉首尾 `"`。 */
export function normalizeOfficialWorldId(raw: unknown): string | undefined {
  let s = String(raw ?? '').trim()
  if (!s) {
    return undefined
  }
  if (s.length >= 2 && s[0] === '"' && s[s.length - 1] === '"') {
    s = s.slice(1, -1).trim()
  }
  return s || undefined
}
