import { defineStore } from 'pinia';
import { ref, computed } from 'vue';
import type { Screenshot } from '@/types/screenshot';
import { screenshotAPI } from '@/api/screenshot';
import { albumAPI } from '@/api/album';

export const useScreenshotListStore = defineStore('screenshotList', () => {
    // State
    const screenshots = ref<Screenshot[]>([]);
    const loading = ref(false);
    const hasMore = ref(true);
    const lastId = ref<number | null>(null);
    const currentAlbumId = ref<number | null>(null);
    const error = ref<string | null>(null);
    
    // 配置
    const batchSize = 50;

    // Getters
    const isEmpty = computed(() => screenshots.value.length === 0);

    // Actions
    async function loadMore() {
        if (loading.value || !hasMore.value) return;
        console.log('Loading more screenshots-store');
        loading.value = true;
        error.value = null;

        try {
            const response = await screenshotAPI.getScreenshots(lastId.value);
            
            if (response.screenshots.length > 0) {
                screenshots.value.push(...response.screenshots);
                lastId.value = screenshots.value[screenshots.value.length - 1].id;
            }

            hasMore.value = response.screenshots.length === 0;
        } catch (e) {
            error.value = e instanceof Error ? e.message : 'Failed to load screenshots';
            console.error('Failed to load screenshots:', e);
        } finally {
            loading.value = false;
        }
    }

    async function loadByMonth(year: number, month: number, reset = false) {
        if (loading.value || (!hasMore.value && !reset)) return;

        if (reset) {
            screenshots.value = [];
            lastId.value = null;
            hasMore.value = true;
        }

        loading.value = true;
        error.value = null;

        try {
            const response = await screenshotAPI.getScreenshotsByMonth({
                year,
                month,
                lastId: lastId.value,
                limit: batchSize
            });

            if (response.screenshots.length > 0) {
                screenshots.value.push(...response.screenshots);
                lastId.value = screenshots.value[screenshots.value.length - 1].id;
            }

            hasMore.value = response.screenshots.length === 0;
        } catch (e) {
            error.value = e instanceof Error ? e.message : 'Failed to load screenshots';
            console.error('Failed to load screenshots:', e);
        } finally {
            loading.value = false;
        }
    }

    function reset() {
        screenshots.value = [];
        loading.value = false;
        hasMore.value = true;
        lastId.value = null;
        currentAlbumId.value = null;
        error.value = null;
    }

    async function loadAlbumPhotos(albumId: number, reset = false) {
        if (loading.value || (!hasMore.value && !reset)) return;

        if (reset || currentAlbumId.value !== albumId) {
            screenshots.value = [];
            lastId.value = null;
            hasMore.value = true;
            currentAlbumId.value = albumId;
        }

        loading.value = true;
        error.value = null;

        try {
            const photos = await albumAPI.getAlbumScreenshots(albumId);
            screenshots.value = photos;
            hasMore.value = false; // 相册照片暂时不支持分页
        } catch (e) {
            error.value = e instanceof Error ? e.message : 'Failed to load album screenshots';
            console.error('Failed to load album screenshots:', e);
        } finally {
            loading.value = false;
        }
    }

    // 添加文件夹相关方法
    async function loadFolderPhotos(folderId: string, relativePath: string, reset = false) {
        if (reset) {
            screenshots.value = [];
            lastId.value = 0;
            hasMore.value = true;
        }

        if (!hasMore.value || loading.value) return;

        loading.value = true;
        try {
            const response = await screenshotAPI.getScreenshots({
                folderId,
                relativePath,
                lastId: lastId.value,
                limit: 20
            });

            screenshots.value.push(...response.screenshots);
            hasMore.value = response.hasMore;
            if (response.screenshots.length > 0) {
                lastId.value = response.screenshots[response.screenshots.length - 1].id;
            }
        } finally {
            loading.value = false;
        }
    }

    return {
        // State
        screenshots,
        loading,
        hasMore,
        error,
        
        // Getters
        isEmpty,
        
        // Actions
        loadMore,
        loadByMonth,
        reset,
        loadAlbumPhotos,
        loadFolderPhotos
    };
}); 