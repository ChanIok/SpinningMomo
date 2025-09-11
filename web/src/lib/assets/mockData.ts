import type { Asset, ListAssetsResponse, AssetStats } from './types'

// Mock 资产数据
export const mockAssets: Asset[] = [
  {
    id: 1,
    filename: 'sunset_landscape.jpg',
    filepath: '/Users/test/Pictures/sunset_landscape.jpg',
    relative_path: 'landscapes/sunset_landscape.jpg',
    type: 'photo',
    width: 3840,
    height: 2160,
    file_size: 2458624,
    mime_type: 'image/jpeg',
    file_hash: 'abc123def456',
    created_at: '2024-01-15T10:30:00Z',
    updated_at: '2024-01-15T10:30:00Z',
  },
  {
    id: 2,
    filename: 'mountain_peak.jpg',
    filepath: '/Users/test/Pictures/mountain_peak.jpg',
    relative_path: 'landscapes/mountain_peak.jpg',
    type: 'photo',
    width: 4096,
    height: 2304,
    file_size: 3145728,
    mime_type: 'image/jpeg',
    file_hash: 'def789abc012',
    created_at: '2024-01-14T14:20:00Z',
    updated_at: '2024-01-14T14:20:00Z',
  },
  {
    id: 3,
    filename: 'city_lights.png',
    filepath: '/Users/test/Pictures/city_lights.png',
    relative_path: 'urban/city_lights.png',
    type: 'photo',
    width: 2560,
    height: 1440,
    file_size: 1536000,
    mime_type: 'image/png',
    file_hash: '789012def345',
    created_at: '2024-01-13T20:15:00Z',
    updated_at: '2024-01-13T20:15:00Z',
  },
  {
    id: 4,
    filename: 'forest_stream.jpg',
    filepath: '/Users/test/Pictures/forest_stream.jpg',
    relative_path: 'nature/forest_stream.jpg',
    type: 'photo',
    width: 3200,
    height: 1800,
    file_size: 2097152,
    mime_type: 'image/jpeg',
    file_hash: '012345abc678',
    created_at: '2024-01-12T08:45:00Z',
    updated_at: '2024-01-12T08:45:00Z',
  },
  {
    id: 5,
    filename: 'ocean_waves.jpg',
    filepath: '/Users/test/Pictures/ocean_waves.jpg',
    relative_path: 'seascapes/ocean_waves.jpg',
    type: 'photo',
    width: 5120,
    height: 2880,
    file_size: 4194304,
    mime_type: 'image/jpeg',
    file_hash: '345678def901',
    created_at: '2024-01-11T16:30:00Z',
    updated_at: '2024-01-11T16:30:00Z',
  },
  {
    id: 6,
    filename: 'desert_dunes.tiff',
    filepath: '/Users/test/Pictures/desert_dunes.tiff',
    relative_path: 'deserts/desert_dunes.tiff',
    type: 'photo',
    width: 6000,
    height: 4000,
    file_size: 8388608,
    mime_type: 'image/tiff',
    file_hash: '678901abc234',
    created_at: '2024-01-10T12:00:00Z',
    updated_at: '2024-01-10T12:00:00Z',
  },
  {
    id: 7,
    filename: 'winter_forest.webp',
    filepath: '/Users/test/Pictures/winter_forest.webp',
    relative_path: 'seasons/winter_forest.webp',
    type: 'photo',
    width: 2048,
    height: 1536,
    file_size: 1048576,
    mime_type: 'image/webp',
    file_hash: '901234def567',
    created_at: '2024-01-09T09:15:00Z',
    updated_at: '2024-01-09T09:15:00Z',
  },
  {
    id: 8,
    filename: 'spring_flowers.jpg',
    filepath: '/Users/test/Pictures/spring_flowers.jpg',
    relative_path: 'seasons/spring_flowers.jpg',
    type: 'photo',
    width: 4608,
    height: 3456,
    file_size: 5242880,
    mime_type: 'image/jpeg',
    file_hash: '234567abc890',
    created_at: '2024-01-08T15:45:00Z',
    updated_at: '2024-01-08T15:45:00Z',
  },
  {
    id: 9,
    filename: 'aurora_borealis.jpg',
    filepath: '/Users/test/Pictures/aurora_borealis.jpg',
    relative_path: 'sky/aurora_borealis.jpg',
    type: 'photo',
    width: 7360,
    height: 4912,
    file_size: 12582912,
    mime_type: 'image/jpeg',
    file_hash: '567890def123',
    created_at: '2024-01-07T22:30:00Z',
    updated_at: '2024-01-07T22:30:00Z',
  },
  {
    id: 10,
    filename: 'waterfall_mist.jpg',
    filepath: '/Users/test/Pictures/waterfall_mist.jpg',
    relative_path: 'waterfalls/waterfall_mist.jpg',
    type: 'photo',
    width: 3840,
    height: 5760,
    file_size: 7340032,
    mime_type: 'image/jpeg',
    file_hash: '890123abc456',
    created_at: '2024-01-06T11:20:00Z',
    updated_at: '2024-01-06T11:20:00Z',
  },
  {
    id: 11,
    filename: 'starry_night.png',
    filepath: '/Users/test/Pictures/starry_night.png',
    relative_path: 'sky/starry_night.png',
    type: 'photo',
    width: 5760,
    height: 3840,
    file_size: 9437184,
    mime_type: 'image/png',
    file_hash: '123456def789',
    created_at: '2024-01-05T01:45:00Z',
    updated_at: '2024-01-05T01:45:00Z',
  },
  {
    id: 12,
    filename: 'tropical_beach.jpg',
    filepath: '/Users/test/Pictures/tropical_beach.jpg',
    relative_path: 'beaches/tropical_beach.jpg',
    type: 'photo',
    width: 4000,
    height: 3000,
    file_size: 3670016,
    mime_type: 'image/jpeg',
    file_hash: '456789abc012',
    created_at: '2024-01-04T13:30:00Z',
    updated_at: '2024-01-04T13:30:00Z',
  },
]

// 生成更多mock数据的函数
export function generateMoreMockAssets(count: number, startId = 13): Asset[] {
  const templates = [
    'landscape_%d.jpg',
    'portrait_%d.png',
    'macro_%d.jpg',
    'street_%d.webp',
    'architecture_%d.tiff',
    'wildlife_%d.jpg',
  ]

  const folders = ['landscapes', 'portraits', 'macro', 'street', 'architecture', 'wildlife']
  const mimeTypes = ['image/jpeg', 'image/png', 'image/webp', 'image/tiff']

  return Array.from({ length: count }, (_, i) => {
    const id = startId + i
    const templateIndex = i % templates.length
    const filename = templates[templateIndex].replace('%d', id.toString())
    const folder = folders[templateIndex]

    return {
      id,
      filename,
      filepath: `/Users/test/Pictures/${filename}`,
      relative_path: `${folder}/${filename}`,
      type: 'photo' as const,
      width: 1920 + Math.floor(Math.random() * 2000),
      height: 1080 + Math.floor(Math.random() * 1500),
      file_size: 1024 * 1024 + Math.floor(Math.random() * 5 * 1024 * 1024), // 1MB - 6MB
      mime_type: mimeTypes[Math.floor(Math.random() * mimeTypes.length)],
      file_hash: Math.random().toString(36).substring(2, 15),
      created_at: new Date(
        Date.now() - Math.floor(Math.random() * 30 * 24 * 60 * 60 * 1000)
      ).toISOString(),
      updated_at: new Date(
        Date.now() - Math.floor(Math.random() * 30 * 24 * 60 * 60 * 1000)
      ).toISOString(),
    }
  })
}

// Mock API 响应
export function createMockListResponse(
  page = 1,
  perPage = 20,
  totalAssets = mockAssets.length
): ListAssetsResponse {
  const start = (page - 1) * perPage
  const end = start + perPage
  const items = mockAssets.slice(start, end)

  return {
    items,
    total_count: totalAssets,
    current_page: page,
    per_page: perPage,
    total_pages: Math.ceil(totalAssets / perPage),
  }
}

// Mock 统计数据
export const mockAssetStats: AssetStats = {
  total_count: 156,
  photo_count: 152,
  video_count: 3,
  live_photo_count: 1,
  total_size: 1024 * 1024 * 1024 * 2.5, // 2.5GB
  oldest_item_date: '2023-06-15T10:30:00Z',
  newest_item_date: '2024-01-15T10:30:00Z',
}

// 根据文件名生成mock缩略图URL
export function getMockThumbnailUrl(assetId: number): string {
  // 使用 Unsplash 作为占位图源
  const width = 400
  const height = 300
  const seed = assetId % 1000
  return `https://picsum.photos/seed/${seed}/${width}/${height}`
}

// 获取mock原图URL
export function getMockAssetUrl(assetId: number): string {
  const width = 1920
  const height = 1080
  const seed = assetId % 1000
  return `https://picsum.photos/seed/${seed}/${width}/${height}`
}
