/**
 * API响应的通用接口
 */
export interface ApiResponse<T> {
    data: T;
    success: boolean;
}

/**
 * 分页响应的通用接口
 * @template T 列表项的类型
 */
export interface PaginatedResponse<T> {
    /** 数据列表 */
    items: T[];
    /** 是否还有更多数据 */
    has_more: boolean;
    /** 总数（可选） */
    total?: number;
}

/**
 * 带消息的成功响应
 */
export interface SuccessResponse {
    success: true;
    data: {
        message: string;
    };
}

/**
 * 错误响应
 */
export interface ErrorResponse {
    success: false;
    data: {
        message: string;
    };
} 