import type { FolderTreeNode } from '@/features/gallery/types'

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
