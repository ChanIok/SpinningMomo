import type { FolderTreeNode, ScanAssetsParams, ScanIgnoreRule } from '@/features/gallery/types'
import { useI18n } from '@/core/i18n'

interface InfinityNikkiAlbumIgnoreRuleTemplate extends Omit<ScanIgnoreRule, 'description'> {
  descriptionKey: string
}

const INFINITY_NIKKI_ALBUM_IGNORE_RULES: InfinityNikkiAlbumIgnoreRuleTemplate[] = [
  {
    pattern: '**',
    patternType: 'glob',
    ruleType: 'exclude',
    descriptionKey: 'plugins.infinityNikki.scanRules.excludeAll',
  },
  {
    pattern: 'X6Game/ScreenShot/**',
    patternType: 'glob',
    ruleType: 'include',
    descriptionKey: 'plugins.infinityNikki.scanRules.includeScreenshot',
  },
  {
    pattern: 'X6Game/Saved/GamePlayPhotos/*/NikkiPhotos_HighQuality/**',
    patternType: 'glob',
    ruleType: 'include',
    descriptionKey: 'plugins.infinityNikki.scanRules.includeHighQualityPhotos',
  },
]

/**
 * 生成 InfinityNikki 游戏相册扫描参数
 * @param gameDir InfinityNikki 游戏根目录
 */
export function createInfinityNikkiAlbumScanParams(gameDir: string): ScanAssetsParams {
  const { t } = useI18n()

  return {
    directory: gameDir.trim(),
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

/**
 * InfinityNikki 游戏照片管理插件
 *
 * 将深层文件夹结构简化为两层：
 * - 第一层：InfinityNikki 文件夹
 * - 第二层：各账号的 NikkiPhotos_HighQuality 文件夹（显示为 UID）和 ScreenShot 文件夹
 */

/**
 * 转换 InfinityNikki 文件夹树结构
 * @param tree 原始文件夹树
 * @returns 转换后的文件夹树
 */
export function transformInfinityNikkiTree(tree: FolderTreeNode[]): FolderTreeNode[] {
  // 在顶层查找 InfinityNikki 节点
  const infinityNikkiIndex = tree.findIndex((node) => node.name === 'InfinityNikki')

  // 如果没有找到 InfinityNikki 节点，返回原始树
  if (infinityNikkiIndex === -1) {
    return tree
  }

  const infinityNikkiNode = tree[infinityNikkiIndex]!

  // 固定路径遍历：InfinityNikki -> X6Game -> Saved -> GamePlayPhotos
  // 期望结构：InfinityNikki/X6Game/Saved/GamePlayPhotos/[UID]/NikkiPhotos_HighQuality
  //          InfinityNikki/X6Game/ScreenShot

  const x6GameNode = infinityNikkiNode.children?.find((node) => node.name === 'X6Game')
  if (!x6GameNode) {
    return tree
  }

  const secondLevelNodes: FolderTreeNode[] = []

  // 处理 Saved/GamePlayPhotos 路径下的照片文件夹
  const savedNode = x6GameNode.children?.find((node) => node.name === 'Saved')
  if (savedNode) {
    const gamePlayPhotosNode = savedNode.children?.find((node) => node.name === 'GamePlayPhotos')
    if (gamePlayPhotosNode?.children) {
      // 遍历每个 UID 文件夹
      for (const uidNode of gamePlayPhotosNode.children) {
        const nikkiPhotosNode = uidNode.children?.find(
          (node) => node.name === 'NikkiPhotos_HighQuality'
        )

        if (nikkiPhotosNode) {
          // 创建新节点，使用 UID 作为显示名称
          secondLevelNodes.push({
            ...nikkiPhotosNode,
            displayName: uidNode.name, // 使用父目录的 UID 作为显示名
            children: [], // 清空子节点
          })
        }
      }
    }
  }

  // 处理 ScreenShot 文件夹
  const screenShotNode = x6GameNode.children?.find((node) => node.name === 'ScreenShot')
  if (screenShotNode) {
    secondLevelNodes.push({
      ...screenShotNode,
      children: [], // 清空子节点
    })
  }

  // 创建新的 InfinityNikki 节点，只包含两层结构
  const newInfinityNikkiNode: FolderTreeNode = {
    ...infinityNikkiNode,
    children: secondLevelNodes,
  }

  // 替换原始树中的 InfinityNikki 节点
  const newTree = [...tree]
  newTree[infinityNikkiIndex] = newInfinityNikkiNode

  return newTree
}
