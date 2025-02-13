import type { Screenshot, ScreenshotParams, ScreenshotListData, MonthStats } from '@/types/screenshot'
import type { ApiResponse } from '@/types/api'
import type { FolderTreeNode } from '@/types/folder'
import { http } from '@/utils/http'

// 截图相关的 API 方法
export const screenshotAPI = {
    // 获取截图列表（支持无限滚动）
    async getScreenshots(params: ScreenshotParams): Promise<ScreenshotListData> {
        const response = await http.get<ApiResponse<ScreenshotListData>>('/screenshots', { params })
        return response.data.data
    },

    // 通过 ID 获取单个截图
    async getScreenshot(id: number): Promise<Screenshot> {
        const response = await http.get<ApiResponse<Screenshot>>(`/screenshots/${id}`)
        return response.data.data
    },

    // 获取相册中的所有截图
    async getAlbumScreenshots(albumId: number): Promise<Screenshot[]> {
        const response = await http.get<ApiResponse<Screenshot[]>>(`/albums/${albumId}/screenshots`);
        return response.data.data;
    },

    // 更新截图信息
    async updateScreenshot(id: number, data: Partial<Screenshot>): Promise<Screenshot> {
        const response = await http.put<ApiResponse<Screenshot>>(`/screenshots/${id}`, data);
        return response.data.data;
    },

    // 删除截图
    async deleteScreenshot(id: number): Promise<void> {
        await http.delete(`/screenshots/${id}`)
    },

    // 获取月份统计信息
    async getMonthStatistics(): Promise<MonthStats[]> {
        const response = await http.get<ApiResponse<MonthStats[]>>('/screenshots/calendar')
        return response.data.data
    },

    // 获取指定月份的照片
    async getScreenshotsByMonth(params: { year: number; month: number } & ScreenshotParams): Promise<ScreenshotListData> {
        const response = await http.get<ApiResponse<ScreenshotListData>>('/screenshots', { params })
        return response.data.data
    },

    // 获取文件夹树
    async getFolderTree(): Promise<FolderTreeNode[]> {
        const response = await http.get<ApiResponse<FolderTreeNode[]>>('/folders/tree')
        return response.data.data
    }
}; 