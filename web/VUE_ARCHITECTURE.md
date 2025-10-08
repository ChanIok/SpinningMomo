# Vue 3 前端架构设计方案

## 📌 架构选型

采用 **Feature-First 混合架构**，结合以下优势：
- ✅ Feature 高内聚（易维护、易扩展）
- ✅ 保留经典分层（components/composables/services）
- ✅ 清晰的依赖层级（避免循环依赖）
- ✅ 适合桌面应用的插件系统

## 📁 目录结构

```
src/
├── main.ts                       # 入口文件
├── App.vue                       # 根组件
├── router/                       # 路由配置
│   ├── index.ts
│   └── guards.ts
│
├── features/                     # 业务功能模块（高内聚）
│   ├── gallery/                  # 画廊功能
│   │   ├── index.ts              # 模块导出
│   │   ├── routes.ts             # 路由定义
│   │   ├── pages/                # 路由页面
│   │   │   └── GalleryPage.vue
│   │   ├── components/           # 功能组件
│   │   │   ├── GalleryGrid.vue
│   │   │   ├── GalleryToolbar.vue
│   │   │   └── GalleryLightbox.vue
│   │   ├── composables/          # 功能逻辑
│   │   │   ├── useGalleryView.ts
│   │   │   └── useGallerySelection.ts
│   │   ├── api.ts                # API 调用
│   │   ├── store.ts              # 状态管理
│   │   └── types.ts              # 类型定义
│   │
│   ├── settings/                 # 设置功能（同样结构）
│   ├── about/                    # 关于页面
│   └── home/                     # 首页
│
├── components/                   # 全局通用组件
│   ├── ui/                       # shadcn-vue 组件库
│   │   ├── button/
│   │   ├── input/
│   │   └── ...
│   └── layout/                   # 布局组件
│       ├── AppLayout.vue
│       ├── AppHeader.vue
│       └── AppSidebar.vue
│
├── composables/                  # 全局组合式函数
│   ├── useTheme.ts               # 主题切换
│   ├── useI18n.ts                # 国际化
│   └── useRpc.ts                 # RPC 通信
│
├── core/                         # 核心基础设施（技术服务层）
│   ├── rpc/                      # RPC 通信层
│   │   ├── core.ts
│   │   └── transport/
│   ├── http/                     # HTTP 客户端
│   ├── i18n/                     # 国际化基础设施
│   ├── env/                      # 环境变量管理
│   └── storage/                  # 本地存储服务
│
├── store/                        # 全局状态管理（Pinia）
│   ├── index.ts
│   └── app.ts                    # 应用级状态（主题、语言等）
│
├── plugins/                      # 插件系统（独立模块）
│   ├── index.ts
│   └── infinity-nikki/
│       ├── index.ts
│       ├── components/
│       ├── api.ts
│       └── types.ts
│
├── types/                        # 全局类型定义
│   ├── global.d.ts
│   ├── common.ts
│   └── webview.d.ts
│
├── utils/                        # 通用工具函数
│   ├── format.ts
│   ├── validation.ts
│   └── helpers.ts
│
└── assets/                       # 静态资源
    ├── styles/
    │   ├── index.css
    │   └── themes/
    ├── images/
    └── fonts/
```

## 🎯 核心设计原则

### 1. 单一职责原则

| 目录 | 职责 | 示例 |
|-----|------|------|
| `features/` | 业务功能实现 | Gallery 的所有相关代码 |
| `components/` | 通用 UI 组件 | Button, Input, Layout |
| `composables/` | 可复用逻辑 | useTheme, useI18n |
| `core/` | 核心基础设施 | RPC 通信、HTTP 请求、本地存储 |
| `store/` | 全局共享状态 | 应用配置、用户信息 |
| `plugins/` | 可插拔模块 | 游戏插件 |

### 2. 依赖层级规则

```
┌─────────────────────────────────┐
│  features (业务层)               │  ← 最高层
│  - 可依赖下面所有层              │
└─────────────────────────────────┘
            ↓ 可依赖
┌─────────────────────────────────┐
│  components + composables       │  ← 共享层
│  - 可依赖 core/utils             │
│  - 不能依赖 features             │
└─────────────────────────────────┘
            ↓ 可依赖
┌─────────────────────────────────┐
│  core + utils (基础层)           │  ← 最底层
│  - 不能依赖上面任何层            │
└─────────────────────────────────┘
```

**禁止反向依赖**，避免循环引用。

### 3. Feature 高内聚设计

每个 feature 是独立的功能单元：

```typescript
// features/gallery/index.ts - 统一导出
export { default as routes } from './routes'
export { useGalleryStore } from './store'
export { galleryApi } from './api'
export * from './types'

// 外部使用
import { routes as galleryRoutes, useGalleryStore } from '@/features/gallery'
```

**优势**：
- ✅ 修改功能只需关注一个目录
- ✅ 易于做代码分割和懒加载
- ✅ 团队协作减少冲突
- ✅ 删除功能只需删除一个目录

### 4. API 层设计模式

```typescript
// features/gallery/api.ts - 业务 API（可组合多个服务）
import { rpcCall } from '@/core/rpc'
import type { GalleryItem, GalleryFilter } from './types'

export const galleryApi = {
  async getItems(filter?: GalleryFilter): Promise<GalleryItem[]> {
    // 可以组合多个底层调用
    const items = await rpcCall('gallery.list', filter)
    const settings = await rpcCall('settings.getGalleryConfig')
    return processItems(items, settings)
  },

  async deleteItem(id: string): Promise<void> {
    return rpcCall('gallery.delete', { id })
  }
}
```

```typescript
// core/rpc/core.ts - 底层 RPC 服务（纯技术实现）
export async function rpcCall<T = any>(
  method: string,
  params?: any
): Promise<T> {
  // RPC 通信逻辑
  // 不关心业务，只负责通信
}
```

**区别**：
- `features/*/api.ts`: **业务 API**，了解业务逻辑，可组合多个调用
- `core/rpc`: **技术服务**，纯通信实现，不懂业务

## 🚀 技术栈

- **构建工具**: Vite 7 + Rolldown
- **框架**: Vue 3.5+ (Vapor Mode)
- **状态管理**: Pinia
- **路由**: Vue Router 4
- **UI 组件**: shadcn-vue + Tailwind CSS 4
- **类型**: TypeScript 5.8+
- **代码规范**: ESLint + Prettier

## 📝 最佳实践

### 1. 文件命名

```
✅ 推荐：
- GalleryPage.vue      (PascalCase for components)
- useGalleryView.ts    (camelCase with 'use' prefix for composables)
- galleryApi.ts        (camelCase for modules)
- types.ts             (lowercase for type files)

❌ 避免：
- gallery-page.vue
- UseGalleryView.ts
- GalleryApi.ts
```

### 2. 路径别名

```typescript
// tsconfig.json
{
  "compilerOptions": {
    "paths": {
      "@/*": ["./src/*"],
      "@/features/*": ["./src/features/*"],
      "@/components/*": ["./src/components/*"]
    }
  }
}
```

### 3. 懒加载

```typescript
// router/index.ts
import { galleryRoutes } from '@/features/gallery'

const router = createRouter({
  routes: [
    ...galleryRoutes, // 自动懒加载
  ]
})

// features/gallery/routes.ts
export default [
  {
    path: '/gallery',
    component: () => import('./pages/GalleryPage.vue'), // 懒加载
    meta: { title: 'Gallery' }
  }
]
```

## 🔄 迁移计划

从 `web_react` 迁移到新架构：

1. **第一阶段**：搭建基础
   - [ ] 配置 Vite + Vue + TypeScript
   - [ ] 集成 shadcn-vue + Tailwind CSS 4
   - [ ] 设置 Pinia + Router
   - [ ] 配置 RPC 通信层

2. **第二阶段**：核心功能
   - [ ] 迁移 Layout 组件
   - [ ] 迁移 Gallery 功能
   - [ ] 迁移 Settings 功能

3. **第三阶段**：完善
   - [ ] 迁移其他页面
   - [ ] 迁移插件系统
   - [ ] 优化性能（Vapor）

## 🎉 总结

这个架构设计：

1. **平衡了简单性和扩展性**
2. **适合桌面应用的复杂场景**
3. **支持插件系统**
4. **易于团队协作**
5. **保持了 Vue 生态的最佳实践**
6. **前后端命名保持一致**（使用 `core/` + `utils/`）

适合你的 SpinningMomo 项目！

