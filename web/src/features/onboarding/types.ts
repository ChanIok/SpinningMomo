export interface InfinityNikkiGameDirResult {
  gameDir?: string
  configFound: boolean
  gameDirFound: boolean
  message: string
}

export interface FileInfoResult {
  path: string
  exists: boolean
  isDirectory: boolean
  isRegularFile: boolean
  isSymlink: boolean
  size: number
  extension: string
  filename: string
  lastModified: number
}
