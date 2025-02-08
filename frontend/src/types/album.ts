export interface Album {
  id: number
  name: string
  description?: string
  cover_screenshot_id?: number
  created_at: number
  updated_at: number
  deleted_at?: number
}

export interface ApiResponse<T> {
  code: number
  message: string
  data: T
} 