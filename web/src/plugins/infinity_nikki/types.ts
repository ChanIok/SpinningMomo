// Infinity Nikki 插件基础类型

// 游戏目录获取结果
export interface InfinityNikkiGameDirResult {
  gameDir?: string
  configFound: boolean
  gameDirFound: boolean
  message: string
}

// 扫描结果
export interface InfinityNikkiScanResult {
  success: boolean
  gameDirectory?: string
  scanResult?: {
    totalFiles: number
    newItems: number
    updatedItems: number
    deletedItems: number
    errors: string[]
    scanDuration: string
  }
  error?: string
}
