export interface Screenshot {
    id: number;
    filename: string;
    filepath: string;
    created_at: string;
    updated_at: string;
    width: number;
    height: number;
    file_size: number;
    metadata?: string;
    deleted_at?: string;
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