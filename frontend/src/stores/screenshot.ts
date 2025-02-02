import { defineStore } from 'pinia';
import { ref, computed } from 'vue';
import type { Screenshot, MonthStats } from '../types/screenshot';
import { screenshotAPI } from '../api/screenshot';
import { http } from '@/utils/http';

export const useScreenshotStore = defineStore('screenshot', () => {
    // State
    const screenshots = ref<Screenshot[]>([]);
    const currentScreenshot = ref<Screenshot | null>(null);
    const loading = ref(false);
    const reachedEnd = ref(false);
    const lastId = ref<number | null>(null);
    const months = ref<MonthStats[]>([]);
    const hasMore = ref(false);
    
    // 配置
    const batchSize = 50;

    // Getters
    const hasScreenshots = computed(() => screenshots.value.length > 0);

    // Actions
    async function loadMoreScreenshots() {
        if (loading.value || reachedEnd.value) {
            console.log('Skip loading - loading:', loading.value, 'reachedEnd:', reachedEnd.value);
            return;
        }

        loading.value = true;
        try {
            console.log('Fetching screenshots with lastId:', lastId.value);
            const response = await screenshotAPI.getScreenshots({
                lastId: lastId.value,
                limit: batchSize
            });
            
            console.log('API response:', {
                hasMore: response.hasMore,
                count: response.screenshots.length,
                batchSize
            });
            
            if (response.screenshots.length > 0) {
                screenshots.value.push(...response.screenshots);
                lastId.value = response.screenshots[response.screenshots.length - 1].id;
                console.log('Updated lastId:', lastId.value);
            }

            // 只有当没有返回数据时才设置 reachedEnd 为 true
            reachedEnd.value = response.screenshots.length === 0;
            console.log('Updated reachedEnd:', reachedEnd.value);

        } catch (error) {
            console.error('Failed to load screenshots:', error);
        } finally {
            loading.value = false;
        }
    }

    async function fetchScreenshot(id: number) {
        loading.value = true;
        try {
            currentScreenshot.value = await screenshotAPI.getScreenshot(id);
            return currentScreenshot.value;
        } catch (error) {
            console.error('Failed to fetch screenshot:', error);
            currentScreenshot.value = null;
            throw error;
        } finally {
            loading.value = false;
        }
    }

    async function deleteScreenshot(id: number) {
        try {
            await screenshotAPI.deleteScreenshot(id);
            screenshots.value = screenshots.value.filter(s => s.id !== id);
            if (currentScreenshot.value?.id === id) {
                currentScreenshot.value = null;
            }
        } catch (error) {
            console.error('Failed to delete screenshot:', error);
            throw error;
        }
    }

    function reset() {
        screenshots.value = [];
        loading.value = false;
        reachedEnd.value = false;
        lastId.value = null;
        months.value = [];
        hasMore.value = false;
    }

    // 获取月份统计信息
    async function fetchMonthStatistics(): Promise<MonthStats[]> {
        const response = await http.get<MonthStats[]>('/api/screenshots/calendar');
        // 获取每个月份的第一张照片信息
        for (const month of response.data) {
            const screenshot = await fetchScreenshot(month.first_screenshot_id);
            month.firstScreenshot = screenshot;
        }
        months.value = response.data;
        return response.data;
    }

    // 获取指定月份的照片
    async function fetchScreenshotsByMonth(year: number, month: number, reset = false) {
        if (reset) {
            screenshots.value = [];
            lastId.value = 0;
            hasMore.value = true;
        }

        if (!hasMore.value || loading.value) return;

        loading.value = true;
        try {
            const response = await http.get<{items: Screenshot[], hasMore: boolean}>('/api/screenshots', {
                params: {
                    year,
                    month,
                    lastId: lastId.value,
                    limit: 20
                }
            });

            screenshots.value.push(...response.data.items);
            hasMore.value = response.data.hasMore;
            if (response.data.items.length > 0) {
                lastId.value = response.data.items[response.data.items.length - 1].id;
            }
        } finally {
            loading.value = false;
        }
    }

    return {
        // State
        screenshots,
        currentScreenshot,
        loading,
        reachedEnd,
        months,
        hasMore,
        
        // Getters
        hasScreenshots,
        
        // Actions
        loadMoreScreenshots,
        fetchScreenshot,
        deleteScreenshot,
        reset,
        fetchMonthStatistics,
        fetchScreenshotsByMonth
    };
}); 