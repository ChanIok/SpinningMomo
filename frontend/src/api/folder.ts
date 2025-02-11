import type { ApiResponse } from '@/types/api'
import type { WatchedFolder, ProcessingProgress } from '@/types/folder'
import { http } from '@/utils/http'

export interface FolderSelectResult {
    path: string
    isAccessible: boolean
}

export const folderAPI = {
    // 获取所有监控文件夹及其状态
    async getAllFolders() {
        const response = await http.get<ApiResponse<Array<{
            path: string,
            progress: ProcessingProgress
        }>>>('/folders')
        return response.data.data
    },

    // 添加监控文件夹
    async addFolder(path: string) {
        const response = await http.post<ApiResponse<string>>('/folders', { path })
        return response.data.data
    },

    // 删除监控文件夹
    async removeFolder(path: string) {
        const response = await http.delete<ApiResponse<string>>(`/folders/${encodeURIComponent(path)}`)
        return response.data.data
    },

    // 获取文件夹处理状态
    async getFolderStatus(path: string) {
        const response = await http.get<ApiResponse<ProcessingProgress>>(`/folders/${encodeURIComponent(path)}/status`)
        return response.data.data
    },

    // 重新处理文件夹
    async reprocessFolder(path: string) {
        const response = await http.post<ApiResponse<string>>(`/folders/${encodeURIComponent(path)}/reprocess`)
        return response.data.data
    },

    // 选择文件夹（调用系统文件夹选择对话框）
    async selectFolder() {
        const response = await http.post<ApiResponse<FolderSelectResult>>('/folders/select')
        return response.data.data
    }
} 