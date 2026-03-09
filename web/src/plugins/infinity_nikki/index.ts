import type { FolderTreeNode, ScanAssetsParams, ScanIgnoreRule } from '@/features/gallery/types'
import { call } from '@/core/rpc'
import { useI18n } from '@/core/i18n'

interface InfinityNikkiAlbumIgnoreRuleTemplate extends Omit<ScanIgnoreRule, 'description'> {
  descriptionKey: string
}

const INFINITY_NIKKI_ALBUM_IGNORE_RULES: InfinityNikkiAlbumIgnoreRuleTemplate[] = [
  {
    pattern: '^.*$',
    patternType: 'regex',
    ruleType: 'exclude',
    descriptionKey: 'plugins.infinityNikki.scanRules.excludeAll',
  },
  {
    pattern: '^[0-9]+/NikkiPhotos_HighQuality(/.*)?$',
    patternType: 'regex',
    ruleType: 'include',
    descriptionKey: 'plugins.infinityNikki.scanRules.includeHighQualityPhotos',
  },
]

function resolveInfinityNikkiAlbumDirectory(gameDir: string): string {
  const normalizedGameDir = gameDir.trim().replace(/[\\/]+$/, '')
  return `${normalizedGameDir}/X6Game/Saved/GamePlayPhotos`
}

interface StartTaskResult {
  taskId: string
}

/**
 * 生成 InfinityNikki 游戏相册扫描参数
 * @param gameDir InfinityNikki 游戏根目录
 */
export function createInfinityNikkiAlbumScanParams(gameDir: string): ScanAssetsParams {
  const { t } = useI18n()

  return {
    directory: resolveInfinityNikkiAlbumDirectory(gameDir),
    generateThumbnails: true,
    thumbnailShortEdge: 480,
    ignoreRules: INFINITY_NIKKI_ALBUM_IGNORE_RULES.map((rule) => ({
      pattern: rule.pattern,
      patternType: rule.patternType,
      ruleType: rule.ruleType,
      description: t(rule.descriptionKey),
    })),
  }
}

export async function startExtractInfinityNikkiPhotoParams(onlyMissing = true): Promise<string> {
  const result = await call<StartTaskResult>('plugins.infinityNikki.startExtractPhotoParams', {
    onlyMissing,
  })
  return result.taskId
}

export async function startInitializeInfinityNikkiScreenshotShortcuts(): Promise<string> {
  const result = await call<StartTaskResult>(
    'plugins.infinityNikki.startInitializeScreenshotShortcuts',
    {}
  )
  return result.taskId
}

/**
 * InfinityNikki 游戏照片管理插件
 *
 * 将深层文件夹结构简化为两层：
 * - 第一层：GamePlayPhotos（显示为大喵相册 / Momo's Album）
 * - 第二层：各账号的 NikkiPhotos_HighQuality 文件夹（显示为 UID）
 */

/**
 * 转换 InfinityNikki 文件夹树结构
 * @param tree 原始文件夹树
 * @returns 转换后的文件夹树
 */
export function transformInfinityNikkiTree(tree: FolderTreeNode[]): FolderTreeNode[] {
  const { t } = useI18n()

  // 在顶层查找 GamePlayPhotos 节点
  const gamePlayPhotosIndex = tree.findIndex((node) => node.name === 'GamePlayPhotos')

  // 如果没有找到 GamePlayPhotos 节点，返回原始树
  if (gamePlayPhotosIndex === -1) {
    return tree
  }

  const gamePlayPhotosNode = tree[gamePlayPhotosIndex]!

  const secondLevelNodes: FolderTreeNode[] = []

  if (gamePlayPhotosNode.children) {
    for (const uidNode of gamePlayPhotosNode.children) {
      if (!/^\d+$/.test(uidNode.name)) {
        continue
      }

      const nikkiPhotosNode = uidNode.children?.find(
        (node) => node.name === 'NikkiPhotos_HighQuality'
      )

      if (nikkiPhotosNode) {
        secondLevelNodes.push({
          ...nikkiPhotosNode,
          displayName: uidNode.name,
          children: [],
        })
      }
    }
  }

  const newGamePlayPhotosNode: FolderTreeNode = {
    ...gamePlayPhotosNode,
    displayName: t('plugins.infinityNikki.album.rootDisplayName'),
    children: secondLevelNodes,
  }

  const newTree = [...tree]
  newTree[gamePlayPhotosIndex] = newGamePlayPhotosNode

  return newTree
}
