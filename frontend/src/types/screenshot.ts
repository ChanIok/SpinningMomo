export interface Screenshot {
    id: number;
    filename: string;
    filepath: string;
    thumbnailPath: string;
    file_size: number;
    width: number;
    height: number;
    created_at: number;
    updated_at: number;
    deleted_at: number;
    thumbnail_generated: boolean;
    metadata: string;
}

export interface ScreenshotResponse {
    screenshots: Screenshot[];
    hasMore: boolean;
}

export interface ScreenshotParams {
    lastId?: number | null;
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

export interface ApiResponse<T> {
    data: T;
    success: boolean;
}

export interface ScreenshotListData {
    screenshots: Screenshot[];
    hasMore: boolean;
} 