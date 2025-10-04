import type { Asset, ListAssetsResponse, AssetStats } from './types'

// Mock 资产数据
export const mockAssets: Asset[] = [
  {
    id: 1,
    name: 'sunset_landscape.jpg',
    path: '/Users/test/Pictures/sunset_landscape.jpg',
    type: 'photo',
    width: 3840,
    height: 2160,
    size: 2458624,
    mimeType: 'image/jpeg',
    hash: 'abc123def456',
    createdAt: new Date('2024-01-15T10:30:00Z').getTime(),
    updatedAt: new Date('2024-01-15T10:30:00Z').getTime(),
  },
  {
    id: 2,
    name: 'mountain_peak.jpg',
    path: '/Users/test/Pictures/mountain_peak.jpg',
    type: 'photo',
    width: 4096,
    height: 2304,
    size: 3145728,
    mimeType: 'image/jpeg',
    hash: 'def789abc012',
    createdAt: new Date('2024-01-14T14:20:00Z').getTime(),
    updatedAt: new Date('2024-01-14T14:20:00Z').getTime(),
  },
  {
    id: 3,
    name: 'city_lights.png',
    path: '/Users/test/Pictures/city_lights.png',
    type: 'photo',
    width: 2560,
    height: 1440,
    size: 1536000,
    mimeType: 'image/png',
    hash: '789012def345',
    createdAt: new Date('2024-01-13T20:15:00Z').getTime(),
    updatedAt: new Date('2024-01-13T20:15:00Z').getTime(),
  },
  {
    id: 4,
    name: 'forest_stream.jpg',
    path: '/Users/test/Pictures/forest_stream.jpg',
    type: 'photo',
    width: 3200,
    height: 1800,
    size: 2097152,
    mimeType: 'image/jpeg',
    hash: '012345abc678',
    createdAt: new Date('2024-01-12T08:45:00Z').getTime(),
    updatedAt: new Date('2024-01-12T08:45:00Z').getTime(),
  },
  {
    id: 5,
    name: 'ocean_waves.jpg',
    path: '/Users/test/Pictures/ocean_waves.jpg',
    type: 'photo',
    width: 5120,
    height: 2880,
    size: 4194304,
    mimeType: 'image/jpeg',
    hash: '345678def901',
    createdAt: new Date('2024-01-11T16:30:00Z').getTime(),
    updatedAt: new Date('2024-01-11T16:30:00Z').getTime(),
  },
  {
    id: 6,
    name: 'desert_dunes.tiff',
    path: '/Users/test/Pictures/desert_dunes.tiff',
    type: 'photo',
    width: 6000,
    height: 4000,
    size: 8388608,
    mimeType: 'image/tiff',
    hash: '678901abc234',
    createdAt: new Date('2024-01-10T12:00:00Z').getTime(),
    updatedAt: new Date('2024-01-10T12:00:00Z').getTime(),
  },
  {
    id: 7,
    name: 'winter_forest.webp',
    path: '/Users/test/Pictures/winter_forest.webp',
    type: 'photo',
    width: 2048,
    height: 1536,
    size: 1048576,
    mimeType: 'image/webp',
    hash: '901234def567',
    createdAt: new Date('2024-01-09T09:15:00Z').getTime(),
    updatedAt: new Date('2024-01-09T09:15:00Z').getTime(),
  },
  {
    id: 8,
    name: 'spring_flowers.jpg',
    path: '/Users/test/Pictures/spring_flowers.jpg',
    type: 'photo',
    width: 4608,
    height: 3456,
    size: 5242880,
    mimeType: 'image/jpeg',
    hash: '234567abc890',
    createdAt: new Date('2024-01-08T15:45:00Z').getTime(),
    updatedAt: new Date('2024-01-08T15:45:00Z').getTime(),
  },
  {
    id: 9,
    name: 'aurora_borealis.jpg',
    path: '/Users/test/Pictures/aurora_borealis.jpg',
    type: 'photo',
    width: 7360,
    height: 4912,
    size: 12582912,
    mimeType: 'image/jpeg',
    hash: '567890def123',
    createdAt: new Date('2024-01-07T22:30:00Z').getTime(),
    updatedAt: new Date('2024-01-07T22:30:00Z').getTime(),
  },
  {
    id: 10,
    name: 'waterfall_mist.jpg',
    path: '/Users/test/Pictures/waterfall_mist.jpg',
    type: 'photo',
    width: 3840,
    height: 5760,
    size: 7340032,
    mimeType: 'image/jpeg',
    hash: '890123abc456',
    createdAt: new Date('2024-01-06T11:20:00Z').getTime(),
    updatedAt: new Date('2024-01-06T11:20:00Z').getTime(),
  },
  {
    id: 11,
    name: 'starry_night.png',
    path: '/Users/test/Pictures/starry_night.png',
    type: 'photo',
    width: 5760,
    height: 3840,
    size: 9437184,
    mimeType: 'image/png',
    hash: '123456def789',
    createdAt: new Date('2024-01-05T01:45:00Z').getTime(),
    updatedAt: new Date('2024-01-05T01:45:00Z').getTime(),
  },
  {
    id: 12,
    name: 'tropical_beach.jpg',
    path: '/Users/test/Pictures/tropical_beach.jpg',
    type: 'photo',
    width: 4000,
    height: 3000,
    size: 3670016,
    mimeType: 'image/jpeg',
    hash: '456789abc012',
    createdAt: new Date('2024-01-04T13:30:00Z').getTime(),
    updatedAt: new Date('2024-01-04T13:30:00Z').getTime(),
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

  const mimeTypes = ['image/jpeg', 'image/png', 'image/webp', 'image/tiff']

  return Array.from({ length: count }, (_, i) => {
    const id = startId + i
    const templateIndex = i % templates.length
    const fileName = templates[templateIndex].replace('%d', id.toString())

    return {
      id,
      name: fileName,
      path: `/Users/test/Pictures/${fileName}`,
      type: 'photo' as const,
      width: 1920 + Math.floor(Math.random() * 2000),
      height: 1080 + Math.floor(Math.random() * 1500),
      size: 1024 * 1024 + Math.floor(Math.random() * 5 * 1024 * 1024), // 1MB - 6MB
      mimeType: mimeTypes[Math.floor(Math.random() * mimeTypes.length)],
      hash: Math.random().toString(36).substring(2, 15),
      createdAt: Date.now() - Math.floor(Math.random() * 30 * 24 * 60 * 60 * 1000),
      updatedAt: Date.now() - Math.floor(Math.random() * 30 * 24 * 60 * 60 * 1000),
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
    totalCount: totalAssets,
    currentPage: page,
    perPage: perPage,
    totalPages: Math.ceil(totalAssets / perPage),
  }
}

// Mock 统计数据
export const mockAssetStats: AssetStats = {
  totalCount: 156,
  photoCount: 152,
  videoCount: 3,
  livePhotoCount: 1,
  totalSize: 1024 * 1024 * 1024 * 2.5, // 2.5GB
  oldestItemDate: '2023-06-15T10:30:00Z',
  newestItemDate: '2024-01-15T10:30:00Z',
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

// 文件夹类型定义
export interface Folder {
  id: string
  name: string
  path: string
  children?: Folder[]
}

// 标签类型定义
export interface Tag {
  id: string
  name: string
  count: number
}

// Mock 文件夹数据
export const mockFolders: Folder[] = [
  {
    id: 'screenshots',
    name: 'Screenshots',
    path: '/Screenshots',
    children: [
      { id: 'screenshots-2024', name: '2024', path: '/Screenshots/2024' },
      { id: 'screenshots-2023', name: '2023', path: '/Screenshots/2023' },
    ],
  },
  {
    id: 'demo',
    name: 'demo',
    path: '/demo',
    children: [
      { id: 'demo-landscapes', name: 'landscapes', path: '/demo/landscapes' },
      { id: 'demo-portraits', name: 'portraits', path: '/demo/portraits' },
      { id: 'demo-nature', name: 'nature', path: '/demo/nature' },
    ],
  },
]

// Mock 标签数据
export const mockTags: Tag[] = [
  { id: 'nature', name: '自然', count: 5 },
  { id: 'landscape', name: '风景', count: 8 },
  { id: 'portrait', name: '人像', count: 3 },
  { id: 'urban', name: '城市', count: 4 },
  { id: 'seasons', name: '季节', count: 2 },
]
