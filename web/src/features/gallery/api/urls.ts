import type { Asset } from '../types'
import { getStaticUrl, isWebView } from '@/core/env'

/**
 * 逐段编码，保留目录层级中的 '/'，同时正确处理中文、空格、#、% 等特殊字符。
 */
function encodeRelativePathForUrl(relativePath: string): string {
  return relativePath
    .split('/')
    .filter((segment) => segment.length > 0)
    .map((segment) => encodeURIComponent(segment))
    .join('/')
}

/**
 * 获取资产缩略图URL
 * 路径格式: thumbnails/[hash前2位]/[hash第3-4位]/{hash}.webp
 */
export function getAssetThumbnailUrl(asset: Asset): string {
  const hash = asset.hash
  if (!hash) {
    return ''
  }

  const prefix1 = hash.slice(0, 2)
  const prefix2 = hash.slice(2, 4)
  const relativePath = `${prefix1}/${prefix2}/${hash}.webp`

  // WebView release 直接走缩略图虚拟主机映射，少一层动态解析。
  if (isWebView() && !import.meta.env.DEV) {
    return `https://thumbs.test/${relativePath}`
  }

  return getStaticUrl(`/static/assets/thumbnails/${relativePath}`)
}

/**
 * 获取资产原图 URL
 *
 * 新模型下，原图由：
 * - rootId：资源属于哪个 watch root
 * - relativePath：文件在该 root 下的相对路径
 * - hash：版本参数，避免内容更新后 URL 不变
 *
 * 环境差异：
 * - WebView：`https://r-<rootId>.test/<relativePath>?v=<hash>`
 * - 浏览器 dev：`/static/assets/originals/by-root/<rootId>/<relativePath>?v=<hash>`
 */
export function getAssetUrl(asset: Asset): string {
  if (!asset.rootId || !asset.relativePath) {
    return ''
  }

  const encodedRelativePath = encodeRelativePathForUrl(asset.relativePath)
  const versionQuery = asset.hash ? `?v=${encodeURIComponent(asset.hash)}` : ''

  if (isWebView()) {
    return `https://r-${asset.rootId}.test/${encodedRelativePath}${versionQuery}`
  }

  return `/static/assets/originals/by-root/${asset.rootId}/${encodedRelativePath}${versionQuery}`
}
