import type { FolderTreeNode } from '@/features/gallery/types'
import { call } from '@/core/rpc'
import { useI18n } from '@/core/i18n'

export const INFINITY_NIKKI_LAST_UID_STORAGE_KEY = 'spinningmomo.infinityNikki.extract.lastUid'

interface StartTaskResult {
  taskId: string
}

interface StartExtractPhotoParamsForFolderParams {
  folderId: number
  uid: string
  onlyMissing?: boolean
}

export async function startExtractInfinityNikkiPhotoParams(onlyMissing = true): Promise<string> {
  const result = await call<StartTaskResult>('extensions.infinityNikki.startExtractPhotoParams', {
    onlyMissing,
  })
  return result.taskId
}

export async function startExtractInfinityNikkiPhotoParamsForFolder(
  params: StartExtractPhotoParamsForFolderParams
): Promise<string> {
  const result = await call<StartTaskResult>(
    'extensions.infinityNikki.startExtractPhotoParamsForFolder',
    params
  )
  return result.taskId
}

export async function startInitializeInfinityNikkiScreenshotHardlinks(): Promise<string> {
  const result = await call<StartTaskResult>(
    'extensions.infinityNikki.startInitializeScreenshotHardlinks',
    {}
  )
  return result.taskId
}

/**
 * 从 Infinity Nikki 标准相册路径中提取 UID。
 * 例如：.../GamePlayPhotos/123456/NikkiPhotos_HighQuality
 */
export function extractInfinityNikkiUidFromFolderPath(path: string): string | null {
  const normalizedPath = path.replace(/\\/g, '/')
  const match = normalizedPath.match(
    /(?:^|\/)GamePlayPhotos\/(\d+)\/NikkiPhotos_HighQuality(?:\/|$)/
  )
  return match?.[1] ?? null
}

/**
 * Infinity Nikki 游戏照片管理拓展
 *
 * 将深层文件夹结构简化为三层：
 * - 第一层：GamePlayPhotos（默认显示为大喵相册 / Momo's Album；若库中有 display_name 则优先）
 * - 第二层：各账号 UID（若库中有 display_name 则优先）
 * - 第三层：照片 / 录像（Videos 的时间目录层级保持隐藏，由 includeSubfolders 聚合）
 */

function pickFolderDisplayName(stored: string | undefined, fallback: string): string {
  const trimmed = stored?.trim()
  return trimmed ? trimmed : fallback
}

function createInfinityNikkiDisplayChild(
  node: FolderTreeNode,
  fallbackDisplayName: string
): FolderTreeNode {
  return {
    ...node,
    displayName: pickFolderDisplayName(node.displayName, fallbackDisplayName),
    children: [],
  }
}

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

  const uidNodes: FolderTreeNode[] = []

  if (gamePlayPhotosNode.children) {
    for (const uidNode of gamePlayPhotosNode.children) {
      if (!/^\d+$/.test(uidNode.name)) {
        continue
      }

      const visibleChildren: FolderTreeNode[] = []

      const nikkiPhotosNode = uidNode.children?.find(
        (node) => node.name === 'NikkiPhotos_HighQuality'
      )
      const videosNode = uidNode.children?.find((node) => node.name === 'Videos')

      if (nikkiPhotosNode) {
        visibleChildren.push(
          createInfinityNikkiDisplayChild(
            nikkiPhotosNode,
            t('extensions.infinityNikki.album.photosDisplayName')
          )
        )
      }

      if (videosNode) {
        visibleChildren.push(
          createInfinityNikkiDisplayChild(
            videosNode,
            t('extensions.infinityNikki.album.videosDisplayName')
          )
        )
      }

      if (visibleChildren.length === 0) {
        continue
      }

      uidNodes.push({
        ...uidNode,
        displayName: pickFolderDisplayName(uidNode.displayName, uidNode.name),
        children: visibleChildren,
      })
    }
  }

  const newGamePlayPhotosNode: FolderTreeNode = {
    ...gamePlayPhotosNode,
    displayName: pickFolderDisplayName(
      gamePlayPhotosNode.displayName,
      t('extensions.infinityNikki.album.rootDisplayName')
    ),
    children: uidNodes,
  }

  const newTree = [...tree]
  newTree[gamePlayPhotosIndex] = newGamePlayPhotosNode

  return newTree
}
