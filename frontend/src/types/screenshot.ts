import type { PaginatedData, ApiResponse, PaginationParams } from './api'

export interface Screenshot {
    id: number;
    filename: string;
    filepath: string;
    width: number;
    height: number;
    file_size: number;
    metadata: string;
    created_at: number;
    updated_at: number;
    deleted_at?: number;
    thumbnailPath?: string;
}

export interface ScreenshotResponse {
    screenshots: Screenshot[];
    hasMore: boolean;
}

export interface ScreenshotParams {
    folderId?: string;
    albumId?: number;
    year?: number;
    month?: number;
    lastId?: string;
    limit?: number;
}

export interface Album {
    id: number;
    name: string;
    description?: string;
    created_at: string;
    updated_at: string;
    screenshot_count: number;
    cover_screenshot_id?: number;
}

export type ScreenshotListData = PaginatedData<Screenshot>

export interface MonthStats {
    year: number;
    month: number;
    count: number;
    first_screenshot_id: number;
    firstScreenshot?: Screenshot;
}

export interface PaginatedResponse<T> {
    items: T[];
    hasMore: boolean;
} 