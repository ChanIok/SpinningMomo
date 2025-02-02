import axios from 'axios';
import type { AxiosInstance } from 'axios';

// 创建 axios 实例
export const http: AxiosInstance = axios.create({
    baseURL: '/api',
    timeout: 10000,
    headers: {
        'Content-Type': 'application/json'
    }
}); 
