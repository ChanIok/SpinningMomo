/**
 * API响应的通用接口
 */
export interface ApiResponse<T> {
    data: T;
    success: boolean;
}

/**
 * 分页数据的通用接口
 */
export interface PaginatedData<T> {
    items: T[];
    hasMore: boolean;
    total?: number;
}

/**
 * 分页请求参数
 */
export interface PaginationParams {
    lastId?: number;
    limit?: number;
}

/**
 * 成功响应消息
 */
export interface SuccessResponse {
    message: string;
}

/**
 * 错误响应消息
 */
export interface ErrorResponse {
    message: string;
} 