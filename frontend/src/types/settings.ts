export interface WatchedFolder {
    path: string
    include_subfolders: boolean
    file_types: string[]
    last_scan: string
}

export interface ThumbnailSettings {
    size: {
        width: number
        height: number
    }
    quality: number
    storage_location: string
}

export interface InterfaceSettings {
    theme: 'light' | 'dark'
    language: string
    default_view_mode: 'grid' | 'list'
    grid_columns: number
}

export interface PerformanceSettings {
    scan_threads: number
    cache_size: number
    preload_images: boolean
}

export interface AppSettings {
    version: string
    watched_folders: WatchedFolder[]
    thumbnails: ThumbnailSettings
    interface: InterfaceSettings
    performance: PerformanceSettings
}

// API 响应类型
export interface ApiResponse<T> {
    code: number
    message: string
    data: T
} 