import axios from 'axios';
import type { Screenshot, ScreenshotParams, ScreenshotResponse, Album } from '../types/screenshot';

// 创建带基础配置的 axios 实例
const api = axios.create({
    baseURL: '/api',
    timeout: 10000,
    headers: {
        'Content-Type': 'application/json'
    }
});

// 截图相关的 API 方法
export const screenshotAPI = {
    // 获取截图列表（支持无限滚动）
    async getScreenshots(params: ScreenshotParams): Promise<ScreenshotResponse> {
        const response = await api.get('/screenshots', { params });
        return response.data;
    },

    // 通过 ID 获取单个截图
    async getScreenshot(id: number): Promise<Screenshot> {
        const response = await api.get(`/screenshots/${id}`);
        return response.data;
    },

    // 获取相册中的所有截图
    async getAlbumScreenshots(albumId: number): Promise<Screenshot[]> {
        const response = await api.get(`/albums/${albumId}/screenshots`);
        return response.data;
    },

    // 更新截图信息
    async updateScreenshot(id: number, data: Partial<Screenshot>): Promise<Screenshot> {
        const response = await api.put(`/screenshots/${id}`, data);
        return response.data;
    },

    // 删除截图
    async deleteScreenshot(id: number): Promise<void> {
        await api.delete(`/screenshots/${id}`);
    }
}; 