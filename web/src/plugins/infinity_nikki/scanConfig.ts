// 扫描忽略规则类型 - 对应后端ScanIgnoreRule
interface ScanIgnoreRule {
  pattern: string
  patternType: 'glob' | 'regex'
  ruleType: 'exclude' | 'include'
  description?: string
}

// 扫描参数类型 - 匹配后端gallery.scanDirectory接口
interface ScanParams {
  directory: string
  generateThumbnails?: boolean
  thumbnailMaxWidth?: number
  thumbnailMaxHeight?: number
  ignoreRules: ScanIgnoreRule[]
}

export function createScanConfig(gameDirectory: string): ScanParams {
  return {
    directory: gameDirectory, // 扫描根目录
    generateThumbnails: true,
    thumbnailMaxWidth: 400,
    thumbnailMaxHeight: 400,
    ignoreRules: [
      // 1. 默认排除所有文件和目录
      {
        pattern: '**',
        patternType: 'glob',
        ruleType: 'exclude',
        description: '默认排除所有文件',
      },

      // 2. 包含游戏截图目录
      // 对应绝对路径：{gameDirectory}/X6Game/ScreenShot/**
      {
        pattern: 'X6Game/ScreenShot/**',
        patternType: 'glob',
        ruleType: 'include',
        description: '包含游戏截图目录',
      },

      // 3. 包含高质量照片目录
      // 对应绝对路径：{gameDirectory}/X6Game/Saved/GamePlayPhotos/{UID}/NikkiPhotos_HighQuality/**
      // 其中 {UID} 是动态的游戏用户ID，使用 * 通配符匹配
      {
        pattern: 'X6Game/Saved/GamePlayPhotos/*/NikkiPhotos_HighQuality/**',
        patternType: 'glob',
        ruleType: 'include',
        description: '包含高质量照片目录',
      },
    ],
  }
}
