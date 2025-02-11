// 处理状态枚举
export type ProcessingStatus = 'pending' | 'processing' | 'completed' | 'failed'

// 处理进度信息
export interface ProcessingProgress {
    status: ProcessingStatus
    total_files: number
    processed_files: number
    current_file: string
    error_message: string
}

// 监控文件夹信息
export interface WatchedFolder {
    path: string
    progress: ProcessingProgress
}

export interface FolderSelectResult {
    path: string
    isAccessible: boolean
}

export interface FolderTreeNode {
  name: string
  full_path: string
  folder_id: string
  photo_count: number
  children?: FolderTreeNode[]
} 