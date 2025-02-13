import type { Album } from '@/types/album';
import type { Screenshot } from '@/types/screenshot';
import type { ApiResponse } from '@/types/api';
import { http } from '@/utils/http';

export const albumAPI = {
    // 获取所有相册列表
    async getAlbums(): Promise<Album[]> {
        const response = await http.get<ApiResponse<Album[]>>('/albums');
        return response.data.data;
    },

    // 创建新相册
    async createAlbum(data: { name: string; description?: string }): Promise<Album> {
        const response = await http.post<ApiResponse<Album>>('/albums', data);
        return response.data.data;
    },

    // 更新相册信息
    async updateAlbum(id: number, data: Partial<Album>): Promise<Album> {
        const response = await http.put<ApiResponse<Album>>(`/albums/${id}`, data);
        return response.data.data;
    },

    // 删除相册
    async deleteAlbum(id: number): Promise<void> {
        await http.delete(`/albums/${id}`);
    },

    // 添加截图到相册
    async addScreenshots(albumId: number, screenshotIds: number[]): Promise<void> {
        await http.post(`/albums/${albumId}/screenshots`, {
            screenshot_ids: screenshotIds
        });
    },

    // 从相册中移除截图
    async removeScreenshot(albumId: number, screenshotId: number): Promise<void> {
        await http.delete(`/albums/${albumId}/screenshots/${screenshotId}`);
    },

    // 获取相册中的所有照片
    async getAlbumScreenshots(albumId: number): Promise<Screenshot[]> {
        const response = await http.get<ApiResponse<Screenshot[]>>(`/albums/${albumId}/screenshots`);
        return response.data.data;
    }
}; 