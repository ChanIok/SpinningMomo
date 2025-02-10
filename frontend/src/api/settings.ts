import type { 
    AppSettings, 
    WatchedFolder, 
    ThumbnailSettings, 
    InterfaceSettings, 
    PerformanceSettings,
    ApiResponse,
    MessageResponse
} from '../types/settings'
import { http } from '@/utils/http'

interface FolderSelectResult {
    path: string;
    isAccessible: boolean;
}

export const settingsAPI = {
    // 获取所有设置
    async getSettings(): Promise<AppSettings> {
        const response = await http.get<ApiResponse<AppSettings>>('/settings')
        return response.data.data
    },

    // 更新所有设置
    async updateSettings(settings: AppSettings): Promise<AppSettings> {
        const response = await http.put<ApiResponse<AppSettings>>('/settings', settings)
        return response.data.data
    },

    // 获取监视文件夹列表
    async getWatchedFolders(): Promise<WatchedFolder[]> {
        const response = await http.get<ApiResponse<WatchedFolder[]>>('/settings/watched-folders')
        return response.data.data
    },

    // 添加监视文件夹
    async addWatchedFolder(folder: WatchedFolder): Promise<ApiResponse<WatchedFolder>> {
        const response = await http.post<ApiResponse<WatchedFolder>>('/settings/watched-folders', folder)
        return response.data
    },

    // 删除监视文件夹
    async removeWatchedFolder(path: string): Promise<ApiResponse> {
        const response = await http.delete<ApiResponse>(`/settings/watched-folders/${encodeURIComponent(path)}`)
        return response.data
    },

    // 更新缩略图设置
    async updateThumbnailSettings(settings: ThumbnailSettings): Promise<ThumbnailSettings> {
        const response = await http.put<ApiResponse<ThumbnailSettings>>('/settings/thumbnail', settings)
        return response.data.data
    },

    // 更新界面设置
    async updateInterfaceSettings(settings: InterfaceSettings): Promise<InterfaceSettings> {
        const response = await http.put<ApiResponse<InterfaceSettings>>('/settings/interface', settings)
        return response.data.data
    },

    // 更新性能设置
    async updatePerformanceSettings(settings: PerformanceSettings): Promise<PerformanceSettings> {
        const response = await http.put<ApiResponse<PerformanceSettings>>('/settings/performance', settings)
        return response.data.data
    },

    // 选择文件夹
    async selectFolder(): Promise<FolderSelectResult> {
        const response = await http.post<ApiResponse<FolderSelectResult>>('/settings/select-folder')
        return response.data.data
    }
} 